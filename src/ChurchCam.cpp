#include <Arduino.h>
#include <esp_mac.h>
#include "globals.h"


// SIM #include <DNSServer.h>  // For captive portal DNS redirect

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
(Input)Controller - joystick and buttons
(Output)Display - screen refresh (has to talk to)
(Output)Visca/ONVIF
Change Logging to LogUtils
Change NetworkUtils to CameraUtils
*/

//Initialize global variables
// Didn't work in Simulator DNSServer dnsServer;
bool FirstTimeSetup = false;
ATEMmin atemSwitcher;
bool InSimulator = false;


// Indexed by HWRev (REV_OLIMEX / REV_SIM).
// B1/B2/B3 = override / recall1 / recall2. The Simulator pinout must avoid the
// hardcoded TFT pins (2,4,5,13,14,15 -- see Display.cpp): pots on ADC1 (32/34/35),
// buttons on 25/26/27.
struct Pinouts_S Pinouts[REV_MODELS]{
  //                P.  T.  Z. B1. B2. B3 LED RST SLC SDA
  { "OLIMEX POE",  33, 35, 36, 32, 14,  5,  2, 255, 16, 13 },  // NOTE LED is on GPIO2 which does nothing on this board
  { "Simulator",   34, 35, 32, 25, 26, 27,  2, 255, 22, 21 },
  // ADC notes (ESP32): ADC2 does NOT work while WiFi is on. ADC1 = GPIO32-39,
  // ADC2 = GPIO0,2,4,12-15,25-27. INPUT_PULLUP works on 14,16-19,21-23,25-27.
};
int HWRev;
Settings settings;

//Perform initial setup on power on
void setup() {
  // Start Serial
  Serial.begin(115200);
  /*
  for (int i = 0; i < 20; i++) {
    delay(100);
    size_t l = Serial.write("hi\n");
    Serial.printf("delaying startup: %d -- %d\n", i, l);
  }*/
  delay(1000);
  logi("######################## Serial Started");

  // Determine if we're in the simulator or not; it has a hard-coded MAC address.
  // Read the efuse base MAC directly -- on core 3.x WiFi.macAddress() returns
  // all-zeros until the WiFi stack is started, which is too late here.
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  logi("MAC: %s", macStr);
  if( strcmp(macStr, "24:0A:C4:00:01:10") == 0 ) {
    InSimulator = true;
    FirstTimeSetup = true;
  }
  logi("InSimulator: %d", InSimulator);

  // Read settings from EEPROM. WIFI settings are stored seperately by the ESP
  if( !InSimulator ) {
    EEPROM.begin(sizeof(settings));  
    EEPROM.get(0, settings);
  }

  // Validate the EEPROM blob. A blank chip, or one written by an older/different
  // Settings layout, won't match the magic+version -> reset to defaults instead
  // of reading garbage. (Bump SETTINGS_VERSION whenever Settings changes.)
  if (settings.magic != SETTINGS_MAGIC || settings.version != SETTINGS_VERSION) {
    logi("EEPROM settings invalid (magic=0x%08x ver=%d) - resetting to defaults",
         settings.magic, settings.version);
    settings = {};
    settings.magic = SETTINGS_MAGIC;
    settings.version = SETTINGS_VERSION;
    FirstTimeSetup = true;
  }
  logi("FirstTimeSetup: %d", FirstTimeSetup);

  networkSetup();
  
  cameraControlSetup();

  logi("Done");
}

void loop() {
  // PSK SIM dnsServer.processNextRequest();

  // Start network services once, in loop context (not on the WiFi/ETH event task)
  networkServicesLoop();

  if (settings.switcherIP[0] != 0 && networkUp() ) {
    atemSwitcher.runLoop();
  }
  
  cameraControlLoop();

  webLoop();
}


