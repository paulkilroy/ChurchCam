// Cooperative subnet scan for VISCA cameras.
//
// The config page's UDP broadcast only finds framed VISCA-over-IP cameras, and TCP
// can't be broadcast at all -- so raw cameras (HuddleCam on UDP 1259, PTZOptics on
// TCP 5678) never showed up. This sweeps every host in the active interface's subnet
// and probes each of the three VISCA dialects, recording whichever answers a power
// inquiry. It runs one host per scanTick() (called from the main loop) so the web
// server task can serve /scanProgress for the progress bar while it works.
#include <Arduino.h>
#include "globals.h"
#include <lwip/sockets.h>
#include <errno.h>
#include <fcntl.h>

volatile bool     g_scanRunning = false;
volatile uint16_t g_scanDone    = 0;
volatile uint16_t g_scanTotal   = 0;

#define SCAN_HOST_CAP      512   // never sweep more than this (guards a wide mask)
#define SCAN_RCV_TIMEO_MS  120   // per-probe reply wait
#define SCAN_CONN_TIMEO_MS 150   // TCP handshake ceiling

static uint32_t scanCur  = 0;    // current host, host-order numeric
static uint32_t scanSelf = 0;    // our own IP, so we skip probing ourselves

static uint32_t ipToU32(IPAddress a) {
  return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | (uint32_t)a[3];
}
static IPAddress u32ToIp(uint32_t v) {
  return IPAddress((uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
}

// PWR inquiry: framed = 8-byte VISCA-over-IP header + payload; raw = payload only.
static const uint8_t PWR_FRAMED[] = { 0x01,0x00,0x00,0x05,0x00,0x00,0x00,0x01, 0x81,0x09,0x04,0x00,0xFF };

// A VISCA power reply is 90 5x 0y FF (raw), optionally behind the 8-byte IP header.
static bool viscaPwrReply(const uint8_t* r, int len, bool framed) {
  int o = framed ? 8 : 0;
  return len >= o + 4 && r[o] == 0x90 && (r[o + 1] & 0xF0) == 0x50
         && (r[o + 2] == 0x02 || r[o + 2] == 0x03);
}

static void setTimeouts(int fd, int ms) {
  struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
  lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

static bool probeUdp(IPAddress ip, uint16_t port, bool framed) {
  int fd = lwip_socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return false;
  setTimeouts(fd, SCAN_RCV_TIMEO_MS);
  struct sockaddr_in a; memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = (uint32_t)ip;
  bool ok = false;
  if (lwip_connect(fd, (struct sockaddr*)&a, sizeof a) == 0) {
    const uint8_t* p = framed ? PWR_FRAMED : PWR_FRAMED + 8;
    int n = framed ? (int)sizeof(PWR_FRAMED) : 5;
    if (lwip_send(fd, p, n, 0) == n) {
      uint8_t r[64];
      int len = lwip_recv(fd, r, sizeof r, 0);
      ok = len > 0 && viscaPwrReply(r, len, framed);
    }
  }
  lwip_close(fd);
  return ok;
}

static bool probeTcp(IPAddress ip, uint16_t port) {
  int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return false;
  bool ok = false;
  int fl = lwip_fcntl(fd, F_GETFL, 0);
  lwip_fcntl(fd, F_SETFL, fl | O_NONBLOCK);         // bounded connect
  struct sockaddr_in a; memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = (uint32_t)ip;
  bool connected = false;
  if (lwip_connect(fd, (struct sockaddr*)&a, sizeof a) == 0) {
    connected = true;
  } else if (errno == EINPROGRESS) {
    fd_set wf; FD_ZERO(&wf); FD_SET(fd, &wf);
    struct timeval tv = { SCAN_CONN_TIMEO_MS / 1000, (SCAN_CONN_TIMEO_MS % 1000) * 1000 };
    if (lwip_select(fd + 1, NULL, &wf, NULL, &tv) > 0) {
      int err = 0; socklen_t el = sizeof err;
      lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el);
      connected = (err == 0);
    }
  }
  if (connected) {
    lwip_fcntl(fd, F_SETFL, fl);                     // back to blocking for the exchange
    setTimeouts(fd, SCAN_RCV_TIMEO_MS);
    if (lwip_send(fd, PWR_FRAMED + 8, 5, 0) == 5) {  // raw VISCA
      uint8_t r[64];
      int len = lwip_recv(fd, r, sizeof r, 0);
      ok = len > 0 && viscaPwrReply(r, len, false);
    }
  }
  lwip_close(fd);
  return ok;
}

// Probe one host for a VISCA camera. A camera may answer several dialects, so we probe
// all three rather than stop at the first: the probes that fail cost the same timeout
// either way, and responsive hosts are rare, so sweeping all three is nearly free in
// wall-clock. We then register ONE entry using the most reliable transport this host
// offers -- priority raw TCP 5678 > raw UDP 1259 > framed UDP 52381. TCP wins because
// our fire-and-forget drive commands can't afford a silently dropped packet (this is
// what fixed .242); raw UDP beats framed because a 1259 responder is a HuddleCam that
// wants raw, while nearly everything also parrots framed 52381 as a fallback. When a
// camera answers more than one way we tag "+N" so the operator sees it's multi-protocol
// and can switch that row's preset before saving.
static void probeHost(IPAddress ip) {
  bool tcp   = probeTcp(ip, 5678);
  bool uraw  = probeUdp(ip, 1259, false);
  bool ufram = probeUdp(ip, 52381, true);
  int  n     = (tcp ? 1 : 0) + (uraw ? 1 : 0) + (ufram ? 1 : 0);
  if (n == 0) return;

  uint16_t    port;
  uint8_t     transport, headers;
  const char *base;
  if (tcp)       { port = 5678;  transport = CAM_TCP; headers = 0; base = "VISCA (TCP)"; }
  else if (uraw) { port = 1259;  transport = CAM_UDP; headers = 0; base = "VISCA (UDP)"; }
  else           { port = 52381; transport = CAM_UDP; headers = 1; base = "VISCA-over-IP"; }

  char name[24];
  if (n > 1) snprintf(name, sizeof name, "%s +%d", base, n - 1);
  else       snprintf(name, sizeof name, "%s", base);
  addDiscovered(ip, port, CAM_VISCA, transport, headers, name);
}

void scanStart() {
  uint32_t ipv = ipToU32(localIP());
  uint32_t m   = ipToU32(subnetMask());
  if (ipv == 0 || m == 0) {   // no network -- nothing to scan
    g_scanRunning = false; g_scanTotal = 0; g_scanDone = 0;
    logw("Subnet scan: no active IP/mask");
    return;
  }
  uint32_t net   = ipv & m;
  uint32_t bcast = net | ~m;
  uint32_t first = net + 1;              // skip the network address
  uint32_t last  = bcast - 1;            // skip the broadcast address
  uint32_t total = (last >= first) ? (last - first + 1) : 0;
  if (total > SCAN_HOST_CAP) total = SCAN_HOST_CAP;
  scanCur  = first;
  scanSelf = ipv;
  // NOTE: the caller resets the discovered list before seeding it with broadcast
  // results, so scanStart() must NOT clear it here -- it only appends sweep finds.
  g_scanTotal   = (uint16_t)total;
  g_scanDone    = 0;
  g_scanRunning = (total > 0);
  logi("Subnet scan: %s / %s -> %u hosts", localIP().toString().c_str(),
       subnetMask().toString().c_str(), (unsigned)total);
}

void scanTick() {
  if (!g_scanRunning) return;
  if (g_scanDone < g_scanTotal) {
    if (scanCur != scanSelf) probeHost(u32ToIp(scanCur));   // don't probe ourselves
    scanCur++;
    g_scanDone++;
  }
  if (g_scanDone >= g_scanTotal) {
    g_scanRunning = false;
    logi("Subnet scan done: %d camera(s) found", numDiscoveredCameras);
  }
}
