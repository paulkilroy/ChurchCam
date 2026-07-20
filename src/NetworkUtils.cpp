#include <Arduino.h>

#include "globals.h"

// Raw lwIP BSD sockets — one socket per camera, protocol fixed when the socket
// is opened, so send/recv never branch on protocol. Works identically over
// Ethernet or WiFi (a socket doesn't know which netif carries it).
//
// UDP cameras use a *connected* datagram socket: connect() on a UDP socket sends
// nothing, it just pins the peer. That gives each camera its own socket (replies
// are attributable), lets plain send()/recv() work with no address, and surfaces
// an unreachable camera as a socket error instead of silent timeouts. The one
// exception is the broadcast/discovery slot, which must stay unconnected to hear
// replies from many cameras.
#include <lwip/sockets.h>
#include <errno.h>
#include <fcntl.h>

#define NETWORK_DEBUG 1

// Bounded, non-blocking-ish behaviour so a slow or dead camera can never freeze
// the main loop for seconds.
#define CAM_IO_TIMEOUT_MS      400   // per recv()/send() (SO_RCVTIMEO/SO_SNDTIMEO)
#define CAM_CONNECT_TIMEOUT_MS 250   // TCP handshake ceiling
#define CAM_RETRY_BACKOFF_MS   2000  // after a failed open, don't retry until this elapses

struct CameraLink {
  int       fd       = -1;
  uint8_t   transport = CAM_UDP; // CAM_UDP | CAM_TCP -- snapshot of the setting, taken once at open
  bool      broadcast = false;  // discovery slot: UDP, unconnected
  IPAddress ip;
  uint16_t  port     = 0;
  IPAddress lastFrom;           // source of the most recent datagram (discovery)
  uint16_t  lastFromPort = 0;   // ...and its source port
  uint32_t  retryAt  = 0;       // millis() before which we won't reopen
};

static CameraLink cameras[NUM_CAMERAS + 1];

// Count of packets handed to the stack -- sampled by the display's TX histogram.
volatile uint32_t g_txCount = 0;

// For debugging packets to strings
void printBytes(byte array[], unsigned int len) {

  if (len > 0) {
    logd( stringBytes( array, len ).c_str() );
  }
}

String stringBytes(byte array[], unsigned int len) {
  int b = 0;
  char buffer[50];

  for (unsigned int i = 0; i < len && b < 46; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[b++] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[b++] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    buffer[b++] = ' ';
  }
  buffer[b++] = '\0';

  return String(buffer);
}

void writeBytes( uint32_t value, byte packet[], int position ) {
  byte b4 = (byte)(value & 0xFFu);         // LSB
  byte b3 = (byte)((value >> 8) & 0xFFu);
  byte b2 = (byte)((value >> 16) & 0xFFu);
  byte b1 = (byte)((value >> 24) & 0xFFu); // MSB

  packet[position++] = b1;
  packet[position++] = b2;
  packet[position++] = b3;
  packet[position++] = b4;
}

static void linkClose( int cameraNumber ) {
  CameraLink &c = cameras[cameraNumber];
  if ( c.fd >= 0 ) {
    lwip_close( c.fd );
    c.fd = -1;
  }
}

// Tear down after a hard error and hold off reopening for the backoff window.
static void linkFail( int cameraNumber ) {
  linkClose( cameraNumber );
  cameras[cameraNumber].retryAt = millis() + CAM_RETRY_BACKOFF_MS;
}

// Bounded TCP connect: non-blocking connect + select() so an unreachable camera
// times out in CAM_CONNECT_TIMEOUT_MS instead of blocking the loop for seconds.
static bool tcpConnectBounded( int fd, struct sockaddr_in *addr ) {
  int flags = lwip_fcntl( fd, F_GETFL, 0 );
  lwip_fcntl( fd, F_SETFL, flags | O_NONBLOCK );

  bool ok = false;
  int r = lwip_connect( fd, (struct sockaddr*)addr, sizeof(*addr) );
  if ( r == 0 ) {
    ok = true;
  } else if ( errno == EINPROGRESS ) {
    fd_set wset;
    FD_ZERO( &wset );
    FD_SET( fd, &wset );
    struct timeval tv;
    tv.tv_sec  = CAM_CONNECT_TIMEOUT_MS / 1000;
    tv.tv_usec = ( CAM_CONNECT_TIMEOUT_MS % 1000 ) * 1000;
    if ( lwip_select( fd + 1, NULL, &wset, NULL, &tv ) > 0 ) {
      int soErr = 0;
      socklen_t len = sizeof( soErr );
      lwip_getsockopt( fd, SOL_SOCKET, SO_ERROR, &soErr, &len );
      ok = ( soErr == 0 );
    }
  }

  // Restore blocking mode so recv()/send() honour SO_RCVTIMEO/SO_SNDTIMEO.
  lwip_fcntl( fd, F_SETFL, flags );
  return ok;
}

// Open (or reuse) the socket for a camera. Protocol/address are read from
// settings once, when the socket is created; settings only change on /save,
// which reboots, so a socket is configured exactly once per lifetime.
int camConnect( int cameraNumber ) {
  CameraLink &c = cameras[cameraNumber];

  if ( c.fd >= 0 ) {
    // Already open -- but reopen if the target moved since we opened. The default
    // active camera can open its socket very early, before settings are configured,
    // pinning a stale ip/port; this also lets a settings change take effect.
    if ( c.ip == settings.cameraIP[cameraNumber] && c.port == settings.cameraPort[cameraNumber] )
      return NETWORK_SUCCESS;                   // UDP persists; live TCP
    linkClose( cameraNumber );
  }
  if ( millis() < c.retryAt ) return NETWORK_ERROR;  // still backing off a dead camera

  c.transport = settings.cameraTransport[cameraNumber];
  c.broadcast = ( cameraNumber == CAMERA_BROADCAST );
  c.ip        = settings.cameraIP[cameraNumber];
  c.port      = settings.cameraPort[cameraNumber];

  c.fd = lwip_socket( AF_INET, c.transport == CAM_TCP ? SOCK_STREAM : SOCK_DGRAM, 0 );
  if ( c.fd < 0 ) {
    loge( "socket() failed for camera %d\n", cameraNumber + 1 );
    c.retryAt = millis() + CAM_RETRY_BACKOFF_MS;
    return NETWORK_ERROR;
  }

  struct timeval tv;
  tv.tv_sec  = CAM_IO_TIMEOUT_MS / 1000;
  tv.tv_usec = ( CAM_IO_TIMEOUT_MS % 1000 ) * 1000;
  lwip_setsockopt( c.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv );
  lwip_setsockopt( c.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv );

  struct sockaddr_in addr;
  memset( &addr, 0, sizeof addr );
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons( c.port );
  addr.sin_addr.s_addr = (uint32_t)c.ip;   // IPAddress -> network-order uint32

  if ( c.broadcast ) {
    int on = 1;
    lwip_setsockopt( c.fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on );
    return NETWORK_SUCCESS;                 // stay unconnected: hear all responders
  }

  // Connected socket for both protocols. TCP does a bounded handshake; UDP just
  // pins the peer (instant, local) so send()/recv() need no address.
  if ( c.transport == CAM_TCP ) {
    logi( "TCP connecting to camera: %d %s port: %d\n", cameraNumber + 1,
          c.ip.toString().c_str(), c.port );
    if ( !tcpConnectBounded( c.fd, &addr ) ) {
      loge( "TCP connect to %s:%d timed out/failed\n", c.ip.toString().c_str(), c.port );
      linkFail( cameraNumber );
      return NETWORK_ERROR;
    }
  } else {
    if ( lwip_connect( c.fd, (struct sockaddr*)&addr, sizeof addr ) != 0 ) {
      loge( "UDP connect (peer bind) to %s:%d failed\n", c.ip.toString().c_str(), c.port );
      linkFail( cameraNumber );
      return NETWORK_ERROR;
    }
  }
  return NETWORK_SUCCESS;
}

int camSend( int cameraNumber, byte packet[], int size ) {
  CameraLink &c = cameras[cameraNumber];
  if ( c.fd < 0 ) return NETWORK_ERROR;

  int written;
  if ( c.broadcast ) {
    struct sockaddr_in addr;
    memset( &addr, 0, sizeof addr );
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons( c.port );
    addr.sin_addr.s_addr = (uint32_t)c.ip;   // e.g. 255.255.255.255
    written = lwip_sendto( c.fd, packet, size, 0, (struct sockaddr*)&addr, sizeof addr );
  } else {
    written = lwip_send( c.fd, packet, size, 0 );  // connected: UDP and TCP identical
  }
  g_txCount++;   // device-activity tally for the display histogram

#ifdef NETWORK_DEBUG
  logd( "Send() Cam[%d] [%s:%s:%d] %s Bytes[%d] [%s] == ", cameraNumber + 1,
      c.transport == CAM_TCP ? "TCP" : "UDP",
      c.ip.toString().c_str(),
      c.port,
      settings.cameraHeaders[cameraNumber] == 0 ? "EX" : "IN",
      written, stringBytes( packet, size ).c_str() );
#endif

  if ( written != size ) {
    loge( "Didn't write enough bytes: %d vs %d\n", size, written );
    // Tear down and reopen on ANY transport's failure. A UDP send only errors
    // when the socket itself is bad (e.g. it was pinned to a stale peer because
    // it opened before settings were configured); reopening re-reads the current
    // ip/port. Without this a broken UDP socket would never self-heal.
    linkFail( cameraNumber );
    return NETWORK_ERROR;
  }
  return NETWORK_SUCCESS;
}

void camClose( int cameraNumber ) {
  linkClose( cameraNumber );
}

// Source IP / port of the most recent datagram on this link (used by discovery).
IPAddress camRemoteIP( int cameraNumber ) {
  return cameras[cameraNumber].lastFrom;
}
uint16_t camRemotePort( int cameraNumber ) {
  return cameras[cameraNumber].lastFromPort;
}

// Read one message into packet[], never more than cap bytes. Returns the byte
// count on success (>0), NETWORK_TIMEOUT (-1) on timeout, or NETWORK_ERROR (0)
// on error/close. Callers that only compare against ERROR/TIMEOUT are unaffected;
// discovery needs the length to parse the reply. Bounded by SO_RCVTIMEO.
int camRecv( int cameraNumber, byte packet[], size_t cap ) {
  CameraLink &c = cameras[cameraNumber];
  if ( c.fd < 0 ) return NETWORK_ERROR;

  int len;
  if ( c.broadcast ) {
    struct sockaddr_in from;
    socklen_t fromLen = sizeof from;
    memset( &from, 0, sizeof from );
    len = lwip_recvfrom( c.fd, packet, cap, 0, (struct sockaddr*)&from, &fromLen );
    if ( len > 0 ) { c.lastFrom = IPAddress( from.sin_addr.s_addr ); c.lastFromPort = ntohs( from.sin_port ); }
  } else {
    len = lwip_recv( c.fd, packet, cap, 0 );   // clamped to cap AND SO_RCVTIMEO
  }

  if ( len < 0 ) {
    if ( errno == EAGAIN || errno == EWOULDBLOCK ) return NETWORK_TIMEOUT;
    loge( "recv() error on camera %d: errno %d\n", cameraNumber + 1, errno );
    if ( c.transport == CAM_TCP ) linkFail( cameraNumber );
    return NETWORK_ERROR;
  }
  if ( len == 0 && c.transport == CAM_TCP ) {   // peer performed orderly shutdown
    linkFail( cameraNumber );
    return NETWORK_ERROR;
  }

#ifdef NETWORK_DEBUG
  logd( "Recv() %d [%s] == ", len, stringBytes( packet, len ).c_str() );
#endif
  return len;   // byte count on success (>0); NETWORK_ERROR/TIMEOUT are 0/-1
}
