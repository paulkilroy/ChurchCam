#include "globals.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESPmDNS.h>

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
      
      // Grasping.. THIS WORKED -- Stopped ETH competing with Wifi.. 
      // see if it still works after I uncomment the rest of this file
      WiFi.setAutoReconnect(false);
      WiFi.mode(WIFI_OFF);
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      logi("%d ETH_GOT_IP", WiFi.getStatusBits());
      logi("ETH_GOT_IP Hostname: %s IP: %s", ETH.getHostname(), ETH.localIP().toString().c_str());
      WiFi.setAutoReconnect(false);
      WiFi.mode(WIFI_OFF);
      requestNetworkServices("ETH_GOT_IP");
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      logi("%d ETH_DISCONNECTED", WiFi.getStatusBits());
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      WiFi.begin();
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
        } else {
          logi("WIFI_STA_DISCONNECT-3 - Starting AP");
          // PSK SIM dnsServer.start(53, "*", WiFi.softAPIP());
          Serial.printf("WIFI_STA_DISCONNECT - AP Mode - SSID for web config: [%s]\n", AP_SSID);
          WiFi.softAP(AP_SSID);
          WiFi.mode(WIFI_AP);  // Enable softAP to access web interface in case of no WiFi
        } 
        break;
      }
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      logi("%d WIFI_STA_GOT_IP", WiFi.getStatusBits());

      logi("WIFI_STA_GOT_IP Hostname: %s IP: %s", WiFi.getHostname(), WiFi.localIP().toString().c_str());
      // Needed? WiFi.mode(WIFI_STA);  // Disable softAP if connection is successful
      requestNetworkServices("WIFI_STA_GOT_IP");
      WiFiWorked = true;
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

/**
 * Setup event callback, ethernet and wifi and static IPs
*/
void networkSetup() {
  // onEvent needs to be the first thing so we can trigger actions off of WiFi Events
  WiFi.onEvent(wifiEventCallback);

  if( !InSimulator ) {
    logi("Starting ETH");
    // Arduino core 3.x needs the full PHY description. Olimex ESP32-PoE:
    // LAN8720, PHY addr 0, MDC=23, MDIO=18, power=GPIO12, clock=GPIO17 output.
    ETH.begin(ETH_PHY_LAN8720, 0, 23, 18, ETH_PHY_POWER, ETH_CLK_MODE);
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

  // Static IP Setup
  if (settings.staticIP && settings.staticIPAddr[0] != 255) {
    logi("Configuring static IP: %s", settings.staticIPAddr.toString().c_str());
    if( getSSID() == "" ) {
      // No WiFi SSID configured -> we are on Ethernet (see "prefer Ethernet" below)
      ETH.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
    } else {
      WiFi.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
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

// The active interface: prefer Ethernet, fall back to WiFi station. Both are
// NetworkInterfaces in core 3.x, so every accessor below is one line and the
// ETH-vs-WiFi choice lives in exactly one place.
static NetworkInterface& activeNet() {
  return ethUp() ? (NetworkInterface&)ETH : (NetworkInterface&)WiFi.STA;
}

const char* getHostname() { return activeNet().getHostname(); }
IPAddress   localIP()     { return activeNet().localIP(); }
IPAddress   subnetMask()  { return activeNet().subnetMask(); }
IPAddress   gatewayIP()   { return activeNet().gatewayIP(); }