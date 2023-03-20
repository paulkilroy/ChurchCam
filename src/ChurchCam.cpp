#include <Arduino.h>
#include "globals.h"
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <WiFi.h>

#include <DNSServer.h>  // For captive portal DNS redirect

#include <EEPROM.h>
#include <ATEMmin.h>

/* TODO New Design
-------------
ChurchCam - Main loop of getting state and passing it through the loops for each file
    -Get joystick pos, button presses, network state, sim state(?)
Settings - Handles webconfig/save eeprom load/save
(Input)ATEM - discovery, atem setup/loop
(I/O??)Network - setup / maint of eth/wifi (wificallbacks) / helper methods for wifi vs eth / DNS / mDNS
(Input)Board - config board / get characteristics
(Input)Joystick - ?
(Output)Display - screen refresh (has to talk to)
(Output)Camera
*/

//Initialize global variables
DNSServer dnsServer;
bool WiFi_Worked = false;
bool FirstTimeSetup = false;
ATEMmin atemSwitcher;
bool InSimulator = false;


struct Pinouts_S Pinouts[REV_MODELS]{
  //                   T.  P.  Z. B1. B2. B3 LED RST SLC SDA
  { "DOIT DevKit V1", 34, 35, 32, 14, 12, 13, 2, 255, 22, 21 },
  { "OMILEX POE",     35, 33, 36,  32,  14, 5,  2, 255, 16, 13 },  // NOTE LED is on GPIO2 which does nothing on this board
  { "WT32-ETH01",     39, 36, 35, 2,  15, 14, 0, 255, 32, 33 },
  { "Heltec WiFi Kit",36, 37, 38, 23, 19, 22, 25, 16, 15, 4 }

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

void networkSetup(const char info[]) {
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
    //atemSwitcher.serialOutput(0x81);
    logi("%s Connecting to ATEM Switcher IP: %s", info, settings.switcherIP.toString().c_str());
  }

  // Start WebServer and WebSocket
  webSetup();
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
void WiFiEvent(WiFiEvent_t event) {
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
        if (WiFi_Worked) {
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
      WiFi_Worked = true;
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
  delay(1000);

  logi("######################## Serial Started");

  logi("Getting MAC");
  if( WiFi.macAddress() == "24:0A:C4:00:01:10" ) {
    InSimulator = true;
    FirstTimeSetup = true;
  }
  logi("InSimulator: %d", InSimulator);

  //Read settings from EEPROM. WIFI settings are stored seperately by the ESP
  if( !InSimulator ) {
    EEPROM.begin(sizeof(settings));  
    EEPROM.get(0, settings);
  }

  if (settings.staticIP && settings.staticIPAddr[0] != 255) {
    logi("Configuring static IP: %s", settings.staticIPAddr.toString());
    WiFi.config(settings.staticIPAddr, settings.staticGateway, settings.staticSubnetMask);
    // TODO BUG need to add ETH.config() here too?
  } else if (settings.staticIP && settings.staticIPAddr[0] == 255) {
    // This is the first time through.. initialize settings to all 0's instead of 255's
    // memset(&settings, 0, sizeof(settings));
    settings = {};
    FirstTimeSetup = true;
  }
  logi("FirstTimeSetup: %d", FirstTimeSetup);

  cameraControlSetup();

  // onEvent needs to be the first thing so we can trigger actions off of WiFi Events
  WiFi.onEvent(WiFiEvent);

  if( !InSimulator ) {
    logi("Starting ETH");
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
    ETH.setHostname(AP_SSID);
    delay(100);

    if( getSSID() == "" ) {
      logi("Starting AP");
      WiFi.softAP(AP_SSID);
      WiFi.mode(WIFI_AP);
    } else {
      // Put WiFi into station mode and make it connect to saved network
      logi("Attempting connection to WiFi Network name (SSID): [%d]", getSSID());
      WiFi.mode(WIFI_STA);
      WiFi.begin();
    }
    delay(100); // Wait to stabalize so I get the ETH_IP event

  } else {
    WiFi.begin("Wokwi-GUEST", "", 6);
  }

  logi("Done");
}

void loop() {
  // PSK SIM dnsServer.processNextRequest();
  if (settings.switcherIP[0] != 0 && networkUp() ) {
    atemSwitcher.runLoop();
  }

  cameraControlLoop();

  webLoop();
}
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
