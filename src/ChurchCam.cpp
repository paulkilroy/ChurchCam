#include <Arduino.h>
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


struct Pinouts_S Pinouts[REV_MODELS]{
  //                   P.  T.  Z. B1. B2. B3 LED RST SLC SDA
  { "OMILEX POE",     33, 35, 36,  32,  14, 5,  2, 255, 16, 13 },  // NOTE LED is on GPIO2 which does nothing on this board
  { "Simulator",      14, 27, 26, 14, 12, 13, 2, 255, 22, 21 },
  { "WT32-ETH01",     36, 39, 35, 2,  15, 14, 0, 255, 32, 33 },
  { "Heltec WiFi Kit",37, 36, 38, 23, 19, 22, 25, 16, 15, 4 }

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

  // Determine if we're in the simulator or not, it has a hard coded MAC address
  logi("Getting MAC");
  if( WiFi.macAddress() == "24:0A:C4:00:01:10" ) {
    InSimulator = true;
    FirstTimeSetup = true;
  }
  logi("InSimulator: %d", InSimulator);

  // Read settings from EEPROM. WIFI settings are stored seperately by the ESP
  if( !InSimulator ) {
    EEPROM.begin(sizeof(settings));  
    EEPROM.get(0, settings);
  }

  // If memory is still set to all 255's then its our first time in here
  if (settings.staticIP && settings.staticIPAddr[0] == 255) {
    // This is the first time through.. initialize settings to all 0's instead of 255's
    // memset(&settings, 0, sizeof(settings));
    settings = {};
    FirstTimeSetup = true;
  }
  logi("FirstTimeSetup: %d", FirstTimeSetup);

  networkSetup();
  
  cameraControlSetup();

  logi("Done");
}

void loop() {
  // PSK SIM dnsServer.processNextRequest();
  if (settings.switcherIP[0] != 0 && networkUp() ) {
    atemSwitcher.runLoop();
  }

  // Network loop() is in the wifiCallback(); from ESP code

  cameraControlLoop();

  webLoop();
}


