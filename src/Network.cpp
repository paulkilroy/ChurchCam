#include "globals.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESPmDNS.h>

bool WiFiWorked = false;

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
        Serial.print(MDNS.IP(i));
        Serial.print(":");
        Serial.print(MDNS.port(i));
        Serial.println(")");
      }
      settings.switcherIP = MDNS.IP(0);
    }
  }
}

/**
 * Some things need to have the network up before we turn them on or they freak out, put those here
*/
void networkSetup(const char info[]) {
  // Set up mDNS hostname so people can go to hostname.local without the IP address
  if (!MDNS.begin(getHostname())) {
    logi("%s mDNS error [%s]", info, getHostname());
  } else {
    logi("%s mDNS responder started: http://%s.local", info, getHostname());
  }

  discoverATEM(info);

  /* if( InSiimulator ) {
    settings.switcherIP[0]=192;
    settings.switcherIP[1]=168;
    settings.switcherIP[2]=50;
    settings.switcherIP[3]=68;
  }
  */
  if (settings.switcherIP[0] == 0) {
    logi("%s ATEM not configured, skipping begin/connect", info);
  } else {
    atemSwitcher.begin(settings.switcherIP);
    atemSwitcher.connect();
    // To enable serial debug for the ATEM code
    //atemSwitcher.serialOutput(0x81);
    logi("%s Connecting to ATEM Switcher IP: %s", info, settings.switcherIP.toString().c_str());
  }

  // Start WebServer and WebSocket
  webSetup();
  // TODO Move to Web.cpp
  webSocketServer.begin();
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
      networkSetup("ETH_GOT_IP");
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
      networkSetup("WIFI_STA_GOT_IP");
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
      networkSetup("AP_STACONNECTED");
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
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
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
    logi("Configuring static IP: %s", settings.staticIPAddr.toString());
    if( getSSID() == "" ) {
      WiFi.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
    } else {
      // TODO Need to test this code
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

bool ethUp() {
  return WiFiGenericClass::getStatusBits() & ETH_CONNECTED_BIT;
}

bool wifiUp() {
  return WiFiGenericClass::getStatusBits() & STA_CONNECTED_BIT;
}

bool hotspotUp() {
  return WiFiGenericClass::getStatusBits() & AP_STARTED_BIT;
}

bool networkUp() {
  return ethUp() || wifiUp() || hotspotUp();
}

const char* getHostname() {
  if (ethUp()) {
    return ETH.getHostname();
  } else {
    return WiFi.getHostname();
  }
}

IPAddress localIP() {
  if (ethUp()) {
    return ETH.localIP();
  } else {
    return WiFi.localIP();
  }
}

IPAddress subnetMask() {
  if (ethUp()) {
    return ETH.subnetMask();
  } else {
    return WiFi.subnetMask();
  }
}

IPAddress gatewayIP() {
  if (ethUp()) {
    return ETH.gatewayIP();
  } else {
    return WiFi.gatewayIP();
  }
}