#include <Arduino.h>
#include "globals.h"
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <WiFi.h>

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>  // For captive portal DNS redirect

#include <EEPROM.h>
#include <ATEMmin.h>

/** 
// For W32-ETH01
#define ETH_PHY_ADDR        1
#define ETH_PHY_TYPE    ETH_PHY_LAN8720
#define ETH_PHY_POWER   16 
#define ETH_PHY_MDC     23
#define ETH_PHY_MDIO    18
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
*/

// For OLIMEX ESP32-POE
// Need to move these to HWRev if there is another ethernet based device
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12

// Important this is after the ETH_* defines to avoid redefining warnings
#include <ETH.h>


//Initialize global variables
DNSServer dnsServer;
bool WiFi_Worked = false;
char ssid[32] = "";
bool FirstTimeSetup = false;
ATEMmin atemSwitcher;
struct Pinouts_S Pinouts[REV_MODELS]{
  //                    T.  P.  Z. B1. B2. B3 LED RST SLC SDA
  { "Heltec WiFi Kit", 36, 37, 38, 23, 19, 22, 25, 16, 15, 4 },
  { "OMILEX POE", 35, 33, 36, 15, 14, 13, 2, 255, 16, 13 },  // NOTE LED is on GPIO2 which does nothing on this board
  { "DOIT DevKit V1", 34, 35, 32, 14, 12, 13, 2, 255, 22, 21 },
  { "WT32-ETH01", 39, 36, 35, 2, 15, 14, 0, 255, 32, 33 }
  // WT32-ETH01 - Docs don't mention SLC/SDA but they seem to be here/l
  //  https://github.com/espressif/arduino-esp32/pull/7237
  // WT32-ETH01 -- ALERT - Pin12 and 4 don't work as a pullup
  //  Name             T   P   Z  BO  R1  R2  L    R   C   D
  // REMEMBER ADC2 does NOT work when WIFI is on
  // ADC1 controls ADC function for pins GPIO32-GPIO39
  // ADC2 controls ADC function for pins GPIO0, 2, 4, 12-15, 25-27
  // using the arduino esp32 build, i've tested all pins and found the following work with
  // INPUT_PULLUP: 14, 16, 17, 18, 19, 21, 22, 23 as expected
  // but these do not: 13, 25, 26, 27, 32, 33

};
int HWRev;
Settings settings;

void discoverATEM() {
  // _switcher_ctrl._udp.local
  if (settings.switcherIP[0] == 0) {
    Serial.printf("ATEM not initialized - Atempting to auto discover atem\n");
    //int n = MDNS.queryService("switcher_ctrl", "udp");
    int n = MDNS.queryService("blackmagic", "tcp");
    //int n = MDNS.queryService("switcher_ctrl", "udp");
    if (n == 0) {
      Serial.println("No ATEM services found");
    } else {
      // This is build for uncomplicated setups with only one ATEM.. should be fine
      Serial.printf("%d ATEM service(s) found - choosing #1\n", n);
      for (int i = 0; i < n; ++i) {
        // Print details for each service found
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

void networkSetup(const char info[]) {
  if (getHostname() == "" || !MDNS.begin(getHostname())) {  //http://esp32.local
    Serial.printf("mDNS error [%s]\n", getHostname());
  } else {
    Serial.printf("%s mDNS responder started: http://%s.local\n", info, getHostname());
  }

  discoverATEM();
  if (settings.switcherIP[0] == 0) {
    Serial.println("ATEM not setup, skipping");
  } else {
    atemSwitcher.begin(settings.switcherIP);
    atemSwitcher.connect();
    Serial.printf("%s Connecting to ATEM Switcher IP: %s\n", info, settings.switcherIP.toString().c_str());
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      drawCommand("ETH Started");
      //ETH.setHostname("ptz-enet");  // PSK BUG TODO Fix this..
      ETH.setHostname(settings.ptzName);
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      drawCommand("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      drawCommand("ETH_GOT_IP");
      Serial.printf("ETH_GOT_IP Hostname: %s IP: %s\n", ETH.getHostname(), ETH.localIP().toString().c_str());
      WiFi.setAutoReconnect(false);
      WiFi.mode(WIFI_OFF);
      networkSetup("ETH_GOT_IP");
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      drawCommand("ETH Disconnected");
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_ETH_STOP:
      drawCommand("ETH Stopped");
      break;
    case SYSTEM_EVENT_WIFI_READY:
      drawCommand("SYSTEM_EVENT_WIFI_READY");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      drawCommand("SYSTEM_EVENT_SCAN_DONE");
      break;
    case SYSTEM_EVENT_STA_START:
      drawCommand("SYSTEM_EVENT_STA_START");
      WiFi.setHostname("ptz-setup"); // settings.ptzName);
      WiFi.setAutoReconnect(true);
      break;
    case SYSTEM_EVENT_STA_STOP:
      drawCommand("SYSTEM_EVENT_STA_STOP");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      drawCommand("SYSTEM_EVENT_STA_CONNECTED");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      {
        drawCommand("SYSTEM_EVENT_STA_DISCONNECTED");
        if (WiFi_Worked) {
          // Keep it simple and avoid AP_STA mode, its very slow and inconsistent. If your wifi suddnely blew up
          // and you need to reconfigure, just restart the device
          drawCommand("STA_DISCONNECT: WiFi was connected so waiting for a reconnect, do not go into AP mode");
        } else if (ETH.linkUp()) {
          drawCommand("STA_DISCONNECT - ETH is UP");
          //Might need
        } else {
          drawCommand("STA_DISCONNECT - Starting AP");
          /*
          uint64_t chipid = ESP.getEfuseMac();
          uint16_t chip2 = (uint16_t)chipid >> 32;
          uint16_t chip3 = (uint16_t)chipid;
          */
          //snprintf(ssid, 17, "PTZ-%04X", chip3);
          snprintf(ssid, 17, "ptz-setup");

          dnsServer.start(53, "*", WiFi.softAPIP());

          Serial.printf("STA_DISCONNECT - AP Mode \"%s\" WiFi for web config\n", ssid);
          WiFi.softAP(ssid);
          WiFi.mode(WIFI_AP);  // Enable softAP to access web interface in case of no WiFi
        }
        break;
      }
    case SYSTEM_EVENT_STA_GOT_IP:
      drawCommand("STA_GOT_IP");

      Serial.printf("STA_GOT_IP Hostname: %s IP: %s\n", WiFi.getHostname(), WiFi.localIP().toString().c_str());
      WiFi.mode(WIFI_STA);  // Disable softAP if connection is successful
      networkSetup("STA_GOT_IP");
      WiFi_Worked = true;
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
      drawCommand("SYSTEM_EVENT_AP_STAIPASSIGNED");
      break;
    case SYSTEM_EVENT_AP_START:
      drawCommand("SYSTEM_EVENT_AP_START");
      break;
    case SYSTEM_EVENT_AP_STOP:
      drawCommand("SYSTEM_EVENT_AP_STOP");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      drawCommand("SYSTEM_EVENT_AP_STACONNECTED");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      drawCommand("SYSTEM_EVENT_AP_STADISCONNECTED");
      break;
    default:
      break;
  }
}

//Perform initial setup on power on
void setup() {
  //Start Serial
  Serial.begin(115200);
  /*
  for (int i = 0; i < 20; i++) {
    delay(100);
    size_t l = Serial.write("hi\n");
    Serial.printf("delaying startup: %d -- %d\n", i, l);
  }*/
  delay(100);
  Serial.println("SETUP - ######################## Serial Started");

  //Read settings from EEPROM. WIFI settings are stored seperately by the ESP
  EEPROM.begin(sizeof(settings));  //Needed on ESP8266 module, as EEPROM lib works a bit differently than on a regular arduino
  EEPROM.get(0, settings);
  Serial.println("1");

  if (settings.staticIP && settings.staticIPAddr[0] != 255) {
    WiFi.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
  } else if (settings.staticIP && settings.staticIPAddr[0] == 255) {
    // This is the first time through.. initialize settings to all 0's instead of 255's
    memset(&settings, 0, sizeof(settings));
    FirstTimeSetup = true;
  }
  Serial.println("2");

  WiFi.onEvent(WiFiEvent);
    Serial.println("3");

  // PSK SIM ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
  //Put WiFi into station mode and make it connect to saved network
  WiFi.mode(WIFI_STA);
  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.println("SETUP - Attempting connection to WiFi Network name (SSID): " + getSSID());
  // PSK SIM WiFi.begin();

  cameraControlSetup();

  webSetup();
}

void loop() {
  dnsServer.processNextRequest();
  if (settings.switcherIP[0] != 0) {
    atemSwitcher.runLoop();
  }
  //Handle web interface
  //server.handleRequest();
  cameraControlLoop();
}

String getSSID() {
#if ESP32
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  return String(reinterpret_cast<const char*>(conf.sta.ssid));
#else
  return WiFi.SSID();
#endif
}
bool networkUp() {
  return ETH.linkUp() || WiFi.status() == WL_CONNECTED;
}

const char* getHostname() {
  if (ETH.linkUp()) {
    return ETH.getHostname();
  } else {
    return WiFi.getHostname();
  }
}

IPAddress localIP() {
  if (ETH.linkUp()) {
    return ETH.localIP();
  } else {
    return WiFi.localIP();
  }
}

IPAddress subnetMask() {
  if (ETH.linkUp()) {
    return ETH.subnetMask();
  } else {
    return WiFi.subnetMask();
  }
}

IPAddress gatewayIP() {
  if (ETH.linkUp()) {
    return ETH.gatewayIP();
  } else {
    return WiFi.gatewayIP();
  }
}
