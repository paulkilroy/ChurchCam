#include "globals.h"
#include <esp_wifi.h>
#include <Preferences.h>   // persist the working ETH clock pin (WROOM=GPIO17 / WROVER=GPIO0)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>   // captive-portal DNS: answers every hostname with the AP IP

// Captive-portal DNS. Runs only while the config hotspot is up (AP mode); it
// resolves every lookup to the AP IP so the phone/laptop's connectivity probe
// lands on our web server, which redirects it to the config page. DNSServer is
// UDP-only, so it links fine alongside PsychicHttp (unlike WebServer).
static DNSServer dnsServer;
static bool dnsRunning = false;

bool WiFiWorked = false;

// Service startup (mDNS, ATEM connect, web server, websocket) must run on the
// main loop, NOT the WiFi/Ethernet event task: those libraries aren't
// thread-safe and the event task must never block (mDNS discovery alone takes
// ~1-2s). The callback only records a request; the loop starts services exactly
// once. The "started" latch also stops the old behaviour of re-begin()-ing every
// server on each GOT_IP / AP-connect, which leaked listening sockets.
static volatile bool servicesRequested = false;
static bool servicesStarted = false;
static const char* servicesReason = "startup";
// Set from the WiFi event task when STA fails and we need the config AP. The AP is
// actually started in loop context (networkServicesLoop) -- calling WiFi.softAP()
// from inside the event callback logs "Starting AP" but doesn't reliably broadcast.
static volatile bool startApRequested = false;
// An ETH<->WiFi handoff changes which interface carries traffic, but existing
// camera sockets stay bound to the old one. Set from the ETH event task and acted
// on in loop context: close every camera link so it rebuilds on the new interface.
static volatile bool resetCamLinksRequested = false;
// The ESP32 often needs a few tries to associate (a transient NO_AP_FOUND /
// AUTH_EXPIRE on the first attempt is common even with the right password). The
// radio's own auto-reconnect (setAutoReconnect(true)) does the retrying; we just
// count the failed attempts and hold off the config-AP fallback until enough have
// failed that it's clearly not transient.
static uint8_t wifiAttempts = 0;
#define MAX_WIFI_ATTEMPTS 5

static void requestNetworkServices(const char* reason) {
  servicesReason = reason;
  servicesRequested = true;
}

// Maybe move to ATEMmin ???
/**
 * Use mDNS to find the any ATEMs on the network
*/
void discoverATEM(const char info[]) {
  // _switcher_ctrl._udp.local
  if (settings.switcherIP[0] == 0) {
    logi("%s ATEM IP not initialized - Atempting to auto discover atem", info);
    //int n = MDNS.queryService("switcher_ctrl", "udp");
    int n = MDNS.queryService("blackmagic", "tcp");
    if (n == 0) {
      logi("%s No ATEM services found", info);
    } else {
      // This is build for uncomplicated setups with only one ATEM.. should be fine
      logi("%s %d ATEM service(s) found - choosing #1", info, n);
      for (int i = 0; i < n; ++i) {
        // Print details for each service found
        Serial.print(info);
        Serial.print("    ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(MDNS.hostname(i));
        Serial.print(" (");
        Serial.print(MDNS.address(i));
        Serial.print(":");
        Serial.print(MDNS.port(i));
        Serial.println(")");
      }
      settings.switcherIP = MDNS.address(0);
    }
  }
}

// In the Wokwi simulator, point the ATEM and the four cameras at the Node
// simulators on the host, reached via the private gateway at
// host.wokwi.internal. Mirrors simulator/server.js:
//   cam1 VISCA-UDP:52381, cam2 VISCA-TCP:52382, cam3/4 ONVIF:8083/8084, ATEM 9910.
static void configureSimulatorTargets(const char info[]) {
  IPAddress host;
  if (!WiFi.hostByName("host.wokwi.internal", host)) {
    logi("%s SIM: could not resolve host.wokwi.internal", info);
    return;
  }
  logi("%s SIM: targeting host sims at %s", info, host.toString().c_str());
  settings.switcherIP = host;
  const uint16_t ports[4]      = { 52381, 52382, 8083, 8084 };
  const uint8_t  types[4]      = { CAM_VISCA, CAM_VISCA, CAM_ONVIF, CAM_ONVIF };
  const uint8_t  transports[4] = { CAM_UDP,   CAM_TCP,   CAM_TCP,   CAM_TCP };
  const uint8_t  headers[4]    = { 1, 0, 1, 1 };
  for (int i = 0; i < 4; i++) {
    settings.cameraIP[i]        = host;
    settings.cameraPort[i]      = ports[i];
    settings.cameraType[i]      = types[i];
    settings.cameraTransport[i] = transports[i];
    settings.cameraHeaders[i]   = headers[i];
  }
  // Camera 3 (index 2) requires auth -- match the sim's ONVIF_USER/ONVIF_PASS.
  // Camera 4 (index 3) is left blank on purpose: the sim runs it anonymously, so
  // the firmware sends no WS-Security header. Exercises both ONVIF auth paths.
  strlcpy(settings.cameraUser[2], "admin",     sizeof(settings.cameraUser[2]));
  strlcpy(settings.cameraPass[2], "churchcam", sizeof(settings.cameraPass[2]));
}

/**
 * Some things need to have the network up before we turn them on or they freak out, put those here
*/
void networkSetup(const char info[]) {
  // In the simulator, target the host's Node camera/ATEM sims before anything
  // that reads switcherIP/cameraIP (discoverATEM, atemSwitcher.connect).
  if (InSimulator) configureSimulatorTargets(info);

  // Kick off SNTP so we have real UTC. ONVIF WS-Security needs a <Created>
  // timestamp within the camera's clock-skew window, and the log timestamps
  // benefit too. configTime is async (non-blocking); time becomes valid a moment
  // later -- callers must tolerate "not synced yet" (see onvifCreatedNow()).
  // UTC only (offset 0, no DST); ONVIF Created is always UTC ("...Z").
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Set up mDNS hostname so people can go to hostname.local without the IP address
  if (!MDNS.begin(getHostname())) {
    logi("%s mDNS error [%s]", info, getHostname());
  } else {
    logi("%s mDNS responder started: http://%s.local", info, getHostname());
  }

  discoverATEM(info);

  if (settings.switcherIP[0] == 0) {
    logi("%s ATEM not configured, skipping begin/connect", info);
  } else {
    atemSwitcher.begin(settings.switcherIP);
    atemSwitcher.connect();
    // To enable serial debug for the ATEM code (WARNING: blocking Serial at
    // 115200 stalls runLoop enough to drop the ATEM keepalive -- leave off)
    //atemSwitcher.serialOutput(0x80);
    logi("%s Connecting to ATEM Switcher IP: %s", info, settings.switcherIP.toString().c_str());
  }

  // Start the PsychicHttp server (HTTP config UI + telemetry WebSocket on :80).
  webSetup();
}

// Called every loop() iteration. Starts network services exactly once, in loop
// context, after the event callback has signalled connectivity — keeping the
// non-thread-safe begin()/connect()/mDNS work off the WiFi/Ethernet event task.
void networkServicesLoop() {
  // Pump the captive-portal DNS while the hotspot is up (must be called often).
  if (dnsRunning) dnsServer.processNextRequest();

  // An interface came or went -- rebuild the camera sockets on the current one.
  if (resetCamLinksRequested) {
    resetCamLinksRequested = false;
    logi("Interface changed -> resetting camera links");
    camResetAllLinks();
  }

  // Start the config AP here (loop context) when the WiFi event task asked for it.
  if (startApRequested) {
    startApRequested = false;
    // Race guard: WiFi can exhaust its retries before ETH finishes coming up (~6s
    // on WROVER). If Ethernet is up by now we have connectivity -- skip the AP.
    if (ethUp()) {
      logi("Config AP requested, but ETH is up now -- skipping AP");
    } else {
      logi("Starting config AP: %s", AP_SSID);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID);
      // Point every DNS lookup at us so the OS captive-portal check pops the page.
      dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
      if (dnsServer.start(53, "*", WiFi.softAPIP())) {
        dnsRunning = true;
        logi("Captive DNS started -> %s", WiFi.softAPIP().toString().c_str());
      } else {
        logw("Captive DNS failed to start");
      }
    }
  }

  if (!servicesRequested || servicesStarted) return;
  servicesStarted = true;
  logi("Starting network services (%s)", servicesReason);
  networkSetup(servicesReason);
}

// Ideas to fix POE
//  -Turn on ESP debugging to see whats happening there -- doesn't matter, I can't see it!
//  -Add a sleep after ETH.begin() -- Already have a 1 sec sleep
//  -Move ETH.setHostname() to setup() -- OLIMEX example does it in ETH_START
//  Worked -Maybe move STA stop/reconnect / stop AHEAD to ETH_CONNECTED from ETH_GOT_IP
//  -Maybe move STA reconnect and/or setHostname BACK from WIFI_START to WIFI_GOT_IP
//  -Before I enter the loop, ETH.begin() then sleep 2 sec (start with 10) then look see if I have an IP address by checking bits
//  -Connect serial port to board without power like WT-ETH0 U0TXD / U0RXD
void wifiEventCallback(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      logi("%d ETH_START", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      logi("%d ETH_CONNECTED", WiFi.getStatusBits());
      // Do NOT toggle the WiFi radio here (that crashed an in-flight camera recv
      // and flapped the link). Both interfaces stay up; ETH's higher route priority
      // (set once after ETH.begin) makes the stack prefer it automatically.
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      logi("%d ETH_GOT_IP", WiFi.getStatusBits());
      logi("ETH_GOT_IP Hostname: %s IP: %s", ETH.getHostname(), ETH.localIP().toString().c_str());
      requestNetworkServices("ETH_GOT_IP");
      resetCamLinksRequested = true;   // traffic now prefers ETH -> rebuild cam sockets on it
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      logi("%d ETH_DISCONNECTED (WiFi stays up as the fallback route)", WiFi.getStatusBits());
      resetCamLinksRequested = true;   // ETH gone -> drop cam sockets so they reopen on WiFi
      break;
    case ARDUINO_EVENT_ETH_STOP:
      logi("%d ETH_STOP", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_READY:
      logi("%d WIFI_READY", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      logi("%d WIFI_SCAN_DONE", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_STA_START:
      logi("%d WIFI_SCAN_START", WiFi.getStatusBits());
      WiFi.setHostname(AP_SSID);
      WiFi.setAutoReconnect(true);
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      logi("%d WIFI_STA_STOP", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      logi("%d WIFI_STA_CONNECTION", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      { 
        logi("%d WIFI_STA_DISCONNECTED", WiFi.getStatusBits());
        if (WiFiWorked) {
          // Keep it simple and avoid AP_STA mode, its very slow and inconsistent. If your wifi suddnely blew up
          // and you need to reconfigure, just restart the device
          logi("WIFI_STA_DISCONNECT-1: WiFi was connected so waiting for a reconnect, do not go into AP mode");
        } else if (ethUp()) {
          logi("WIFI_STA_DISCONNECT-2 - ETH is UP, No need for AP");
        } else if (++wifiAttempts < MAX_WIFI_ATTEMPTS) {
          // Transient failure -- let the radio's auto-reconnect retry before AP.
          logi("WIFI_STA_DISCONNECT - attempt %d/%d failed, retrying", wifiAttempts, MAX_WIFI_ATTEMPTS);
        } else {
          // Out of retries -- flag the config AP for loop context (can't start here).
          logi("WIFI_STA_DISCONNECT-3 - %d attempts failed, requesting config AP [%s]", wifiAttempts, AP_SSID);
          startApRequested = true;
        }
        break;
      }
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      logi("%d WIFI_STA_GOT_IP", WiFi.getStatusBits());

      logi("WIFI_STA_GOT_IP Hostname: %s IP: %s", WiFi.getHostname(), WiFi.localIP().toString().c_str());
      // Needed? WiFi.mode(WIFI_STA);  // Disable softAP if connection is successful
      requestNetworkServices("WIFI_STA_GOT_IP");
      WiFiWorked = true;
      wifiAttempts = 0;
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      logi("%d WIFI_AP_STAIPASSIGNED", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_AP_START:
      logi("%d WIFI_AP_START", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      logi("%d WIFI_AP_STOP", WiFi.getStatusBits());
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      logi("%d WIFI_AP_STACONNECTED", WiFi.getStatusBits());
      requestNetworkServices("AP_STACONNECTED");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      logi("%d WIFI_AP_STADISCONNECTED", WiFi.getStatusBits());
      break;
    default:
      logi("%d UNKNOWN WIFI/ETH STATE", WiFi.getStatusBits());
      break;
  }
}

// Bring up the Ethernet PHY with a specific RMII clock-out pin. Holds the PHY
// power pin (GPIO12) LOW first so the clock is up before the PHY is powered (see
// the sequencing note in networkSetup). Returns true if the driver installed.
static bool ethBeginWithClock(eth_clock_mode_t clkMode) {
  pinMode(ETH_PHY_POWER, OUTPUT);
  digitalWrite(ETH_PHY_POWER, LOW);
  delay(200);
  return ETH.begin(ETH_PHY_LAN8720, 0, 23, 18, ETH_PHY_POWER, clkMode);
}

/**
 * Setup event callback, ethernet and wifi and static IPs
*/
void networkSetup() {
  // onEvent needs to be the first thing so we can trigger actions off of WiFi Events
  WiFi.onEvent(wifiEventCallback);

  if( !InSimulator ) {
    logi("Starting ETH");
    // Olimex ESP32-PoE / PoE-ISO Ethernet: LAN8720, PHY addr 0, MDC=23, MDIO=18,
    // GPIO12 the PHY power-enable pin. The ESP32 generates the 50MHz RMII clock,
    // but WHICH pin it comes out on depends on the module:
    //   - WROOM  (no PSRAM):  GPIO16/17 are free -> clock on GPIO17.
    //   - WROVER (has PSRAM):  PSRAM owns GPIO16/17 -> clock on GPIO0.
    // We deliberately do NOT enable PSRAM in the build (enabling it reserves
    // GPIO16/17 and then even the WROOM's GPIO17 clock is rejected by the EMAC), so
    // psramFound() can't tell the modules apart. Instead we try one clock and fall
    // back to the other. Trying the wrong one first costs an extra failed begin +
    // ETH.end() + delays (~seconds), so we PERSIST the clock that worked (NVS) and
    // try it first next boot -- a given board never changes module. Default when
    // nothing is saved is GPIO0, since the shipping board is the WROVER (Board A).
    //
    // Power SEQUENCING (from Olimex's ESP32-POE-ISO-eth-wifi-nat.ino): GPIO12 is
    // also the MTDI strapping pin and can glitch high at boot, briefly powering
    // the PHY before its clock exists and latching it dead. So each attempt drives
    // GPIO12 LOW (PHY off) and holds before ETH.begin() brings the clock up.
    Preferences ethPrefs;
    ethPrefs.begin("eth", false);
    uint8_t savedClk = ethPrefs.getUChar("clk", 0xFF);      // 0xFF = nothing saved yet
    eth_clock_mode_t first  = (savedClk != 0xFF) ? (eth_clock_mode_t)savedClk
                                                 : ETH_CLOCK_GPIO0_OUT;   // WROVER default
    eth_clock_mode_t second = (first == ETH_CLOCK_GPIO0_OUT) ? ETH_CLOCK_GPIO17_OUT
                                                             : ETH_CLOCK_GPIO0_OUT;
    eth_clock_mode_t usedClk = first;
    bool ethOk = ethBeginWithClock(first);
    if (!ethOk) {
      logi("ETH clock mode %d failed; retrying mode %d", (int)first, (int)second);
      ETH.end();
      delay(300);
      usedClk = second;
      ethOk = ethBeginWithClock(second);
    }
    if (ethOk) {
      logi("ETH.begin OK (clock mode %d)", (int)usedClk);
      // Make Ethernet the preferred default route. By default WiFi STA has a higher
      // stack route priority (100) than ETH (50), so with both interfaces up the
      // stack would egress via WiFi even though the app treats ETH as primary
      // (activeNet()). Bump ETH above WiFi once; the stack then auto-selects ETH as
      // the default whenever its link/IP is up and falls back to WiFi when ETH
      // drops -- no per-event pointer juggling, and no interface teardown.
      ETH.setRoutePrio(200);
      if (savedClk != (uint8_t)usedClk) {
        ethPrefs.putUChar("clk", (uint8_t)usedClk);         // remember the winner
        logi("Persisted working ETH clock mode %d for next boot", (int)usedClk);
      }
    } else {
      loge("ETH.begin failed on both clocks -- continuing on Wi-Fi");
    }
    ethPrefs.end();
    ETH.setHostname(AP_SSID);
    delay(100);

    // If no SSID defined, go into AccesswPoint mode, otherwise try to connect
    if( getSSID() == "" ) {
      logi("Starting AP");
      WiFi.softAP(AP_SSID);
      WiFi.mode(WIFI_AP);
    } else {
      // Put WiFi into station mode and make it connect to saved network
      logi("Attempting connection to WiFi Network name (SSID): [%s]", getSSID().c_str());
      WiFi.mode(WIFI_STA);
      WiFi.begin( getSSID().c_str(), getPSK().c_str() );
    }
    delay(100); // Wait to stabalize so I get the ETH_IP event
  } else {
    WiFi.begin("Wokwi-GUEST", "", 6);
  }

  // Static IP Setup. Ethernet is the primary interface, so the device's static IP
  // binds to ETH on real hardware; WiFi stays on DHCP as the fallback. (Applying it
  // to WiFi instead would put the fixed address on the non-preferred interface while
  // ETH -- the default route -- ran on DHCP.) The simulator has no ETH, so there it
  // configures WiFi.
  if (settings.staticIP && settings.staticIPAddr[0] != 255) {
    logi("Configuring static IP on %s: %s", InSimulator ? "WiFi" : "ETH",
         settings.staticIPAddr.toString().c_str());
    if (InSimulator) {
      WiFi.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
    } else {
      ETH.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
    }
  }
}

// Helper Methods below here to do the following:
//  1) Prefer Settings object over values stored in ESP private memory 
//      SSID/PSK encrypted there but it is difficult to restart and get to them
//  2) Prefer Ethernet over WiFi - there are two NICs on the ESP, if both are active
//      default to ethernet
String getSSID() {
  return settings.ssid;
}

String getPSK() {
  return settings.psk;
}

// "Up" means the interface has a routable IP, not merely that the PHY link is
// connected -- you can't send a packet without an address. This keeps ethUp()/
// wifiUp() consistent with the Web status ladder and stops activeNet() from ever
// selecting a link that's up but still mid-DHCP (localIP() == 0.0.0.0).
bool ethUp() {
  return ETH.hasIP();
}

bool wifiUp() {
  return WiFi.STA.hasIP();
}

bool hotspotUp() {
  return WiFi.AP.started();
}

bool networkUp() {
  return ethUp() || wifiUp() || hotspotUp();
}

// The active interface = whichever one the network stack has chosen as the default
// route. We bumped ETH's route priority above WiFi (see ETH.setRoutePrio above), so
// this is Ethernet when its link is up, WiFi when ETH is down, and the AP when only
// the hotspot is up -- one source of truth, the same the routing uses. Falls back to
// WiFi.STA if the stack has no default yet (very early boot).
static NetworkInterface& activeNet() {
  NetworkInterface* def = Network.getDefaultInterface();
  return def ? *def : (NetworkInterface&)WiFi.STA;
}

// Fixed to AP_SSID so the name is consistent everywhere: the AP, the mDNS name
// (ptz-setup.local), the DHCP hostname set on the interfaces, and the display
// header. The interface's own getHostname() reports the MAC-derived default
// (esp32-XXXX) even after setHostname(), which made mDNS come up as esp32-XXXX.local.
const char* getHostname() { return AP_SSID; }
IPAddress   localIP()     { return activeNet().localIP(); }
IPAddress   subnetMask()  { return activeNet().subnetMask(); }
IPAddress   gatewayIP()   { return activeNet().gatewayIP(); }