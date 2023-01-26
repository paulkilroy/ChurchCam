// https://github.com/ThingPulse/esp8266-oled-ssd1306

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <ETH.h>

#include "globals.h"

/*
  U8g2lib Example Overview:
    Frame Buffer Examples: clearBuffer/sendBuffer. Fast, but may not work with all Arduino boards because of RAM consumption
    Page Buffer Examples: firstPage/nextPage. Less RAM usage, should work with all Arduino boards.
    U8x8 Text Only Example: No RAM usage, direct communication with display controller. No graphics, 8x8 Text only.

*/

String CommandBuffer[] = { "", "", "" };

// Please UNCOMMENT one of the contructor lines below
// U8g2 Contructor List (Frame Buffer)
// The complete list is available here: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
// Please update the pin numbers according to your setup. Use U8X8_PIN_NONE if the reset pin is not connected

U8G2 *u8g2;

// Heltec Wifi Kit 32
//U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 16, 15, 4);

// AliExpress SH1106 1.3" OLED
// PK U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// NOTE These must be XBM not BMP format..

#define ptz_setup_width 64
#define ptz_setup_height 64
const unsigned char ptz_setup_bits[] = {
  0xFF, 0xFF, 0x73, 0x80, 0x71, 0xC6, 0xFF, 0xFF, 0xFF, 0xFF, 0x63, 0x80, 
  0x33, 0xCE, 0xFF, 0xFF, 0x9F, 0xDD, 0x63, 0xDA, 0x23, 0xC4, 0x9D, 0xED, 
  0x03, 0x80, 0x83, 0xFF, 0x01, 0xC0, 0x01, 0xE0, 0x07, 0x80, 0x83, 0xFF, 
  0x01, 0xC0, 0x01, 0xC0, 0xE3, 0x1F, 0xF3, 0x7F, 0x0E, 0xC6, 0xF1, 0xE7, 
  0xE7, 0x9F, 0xE3, 0x7F, 0x0E, 0xCE, 0xF8, 0xE7, 0xE7, 0x1F, 0xF3, 0x7F, 
  0x2E, 0xC7, 0xF9, 0xE7, 0xE3, 0x9F, 0xF3, 0x7F, 0xFE, 0xCF, 0xF9, 0xE7, 
  0xE7, 0x1F, 0xE3, 0x7F, 0xFE, 0xC7, 0xF8, 0xC7, 0xE3, 0x9F, 0x73, 0x02, 
  0xCE, 0xCF, 0xF9, 0xE7, 0xE7, 0x9F, 0x73, 0x00, 0xCE, 0xC7, 0xF9, 0xE7, 
  0xE7, 0x8F, 0x63, 0x00, 0x84, 0xCF, 0xF8, 0xE7, 0x03, 0x00, 0x03, 0x0C, 
  0x00, 0xCE, 0x01, 0xC0, 0x07, 0x80, 0x03, 0x0C, 0x00, 0xC6, 0x01, 0xE0, 
  0xDF, 0xB6, 0x63, 0x8E, 0x11, 0xCE, 0xDB, 0xF6, 0xFF, 0xFF, 0x63, 0x8C, 
  0x73, 0xCE, 0xFF, 0xFF, 0xFF, 0xFF, 0x73, 0x8E, 0x31, 0xC6, 0xFF, 0xFF, 
  0x00, 0x00, 0x00, 0x0C, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 
  0xF0, 0x01, 0x00, 0x00, 0xAA, 0x01, 0x41, 0xBC, 0xE0, 0x11, 0x50, 0xC2, 
  0xFF, 0x83, 0x73, 0xFE, 0x81, 0x39, 0xF8, 0xC7, 0xFF, 0x83, 0x73, 0xFC, 
  0xC1, 0x31, 0xF8, 0xE7, 0x87, 0x7F, 0x60, 0xFE, 0x0F, 0x38, 0x06, 0xE0, 
  0x03, 0xFF, 0x60, 0xFC, 0x0F, 0x30, 0x0E, 0xE0, 0x83, 0x5F, 0x60, 0xFF, 
  0x07, 0x3A, 0x07, 0x82, 0xE0, 0x1F, 0x83, 0x7F, 0x00, 0x3E, 0x06, 0x07, 
  0xE0, 0x9F, 0x83, 0x7F, 0x00, 0x3E, 0x07, 0x07, 0x00, 0x00, 0x8C, 0x1F, 
  0xFE, 0x0F, 0xF8, 0x3F, 0x00, 0x00, 0x8C, 0x0F, 0xFE, 0x0F, 0xF8, 0x1F, 
  0x00, 0x00, 0x9D, 0x0F, 0xFE, 0x0F, 0xF8, 0x1F, 0xE0, 0x80, 0x83, 0x0F, 
  0xFE, 0xFF, 0xF9, 0x00, 0xE0, 0x80, 0x83, 0x0F, 0xFE, 0xFF, 0xF9, 0x00, 
  0x78, 0xF8, 0xE0, 0xF3, 0x61, 0xFE, 0xBF, 0x1F, 0xF8, 0x7C, 0xF0, 0xF3, 
  0x01, 0xFE, 0x3F, 0x3F, 0xFC, 0xF8, 0xF0, 0xE3, 0x01, 0xFE, 0x3F, 0x1E, 
  0x18, 0xE0, 0xE3, 0x81, 0x01, 0x00, 0xFF, 0x1F, 0x1C, 0xE0, 0xF3, 0x83, 
  0x01, 0x00, 0xFE, 0x3F, 0xAB, 0xA7, 0xF6, 0xF4, 0x31, 0xD0, 0xFF, 0x3D, 
  0xE3, 0x1F, 0x7C, 0xFE, 0x33, 0xF8, 0xFF, 0x18, 0xE3, 0x1F, 0x7C, 0xFE, 
  0x33, 0xF8, 0xFF, 0x39, 0xE0, 0xE0, 0x9F, 0x81, 0xCF, 0xFF, 0xFF, 0x07, 
  0xE0, 0xE0, 0x8F, 0x83, 0xCF, 0xFF, 0xFF, 0x07, 0x40, 0x60, 0xB5, 0x61, 
  0xCF, 0x5F, 0x7E, 0x05, 0x00, 0x00, 0x60, 0x70, 0x8E, 0x0F, 0x38, 0x18, 
  0x00, 0x00, 0x70, 0x30, 0xCC, 0x0F, 0x38, 0x38, 0xFF, 0xFF, 0x83, 0xCF, 
  0x73, 0xC6, 0xF8, 0xFF, 0xFF, 0xFF, 0x83, 0x8F, 0x31, 0xCE, 0xF9, 0xFF, 
  0xDF, 0xB6, 0x03, 0x9B, 0x21, 0x8E, 0xF8, 0xB7, 0x07, 0x80, 0x03, 0x70, 
  0x00, 0x0E, 0xF8, 0x00, 0x03, 0x80, 0x03, 0x70, 0x00, 0x06, 0xF8, 0x00, 
  0xE7, 0x1F, 0x83, 0x81, 0x31, 0xFE, 0xFF, 0xFF, 0xE7, 0x9F, 0x83, 0x83, 
  0x71, 0xFE, 0xFF, 0xFF, 0xE3, 0x0F, 0x83, 0x81, 0x31, 0xFE, 0xFF, 0xFF, 
  0xE7, 0x9F, 0x73, 0x70, 0x70, 0xFE, 0x39, 0xF8, 0xE7, 0x9F, 0x73, 0x70, 
  0x30, 0xFE, 0x38, 0xF8, 0xE3, 0x9F, 0xF3, 0xFF, 0x01, 0xD0, 0x3F, 0x38, 
  0xE7, 0x1F, 0xE3, 0xFF, 0x01, 0xC0, 0x3F, 0x38, 0xE7, 0x9F, 0xF3, 0xFF, 
  0x03, 0xC0, 0x3F, 0x18, 0x03, 0x80, 0x73, 0x8C, 0xCF, 0x01, 0x3F, 0x07, 
  0x07, 0x80, 0x73, 0x8E, 0xCF, 0x01, 0x3E, 0x07, 0xBF, 0xAB, 0x63, 0x8C, 
  0x5F, 0x10, 0x3E, 0x9F, 0xFF, 0xFF, 0x73, 0x0C, 0x3E, 0x30, 0x38, 0xFF, 
  0xFF, 0xFF, 0x73, 0x0E, 0x3E, 0x38, 0x38, 0xFF, };

#define wifi_width 18
#define wifi_height 17
const uint8_t wifi_bits[] = {
  0x00, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0xf8, 0x7f, 0x00, 0x1e, 0xe0, 0x01,
  0x06, 0x80, 0x01, 0xc0, 0x0f, 0x00, 0xf0, 0x3f, 0x00, 0x3c, 0xf0, 0x00,
  0x0c, 0xc0, 0x00, 0x80, 0x07, 0x00, 0xe0, 0x1f, 0x00, 0x78, 0x78, 0x00,
  0x18, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00
};

#define atem_width 18
#define atem_height 17
const uint8_t atem_bits[] = {
  0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x36, 0x00, 0x00, 0x36, 0x00, 0x00,
  0xf7, 0xff, 0x03, 0x36, 0x00, 0x00, 0x36, 0x00, 0x00, 0x3e, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0xb0, 0x01, 0x00, 0xb0, 0x01,
  0xff, 0xbf, 0x03, 0x00, 0xb0, 0x01, 0x00, 0xb0, 0x01, 0x00, 0xf0, 0x01,
  0x00, 0x00, 0x00
};

#define cam_width 19
#define cam_height 19
const uint8_t cam_bits[] = {
  0xfe, 0x3f, 0x00, 0xff, 0x7f, 0x06, 0x03, 0x60, 0x07, 0x03, 0xe0, 0x07,
  0x03, 0xe0, 0x06, 0x03, 0x60, 0x06, 0x03, 0x60, 0x06, 0x03, 0x60, 0x06,
  0x03, 0x60, 0x06, 0x03, 0x60, 0x06, 0x03, 0x60, 0x06, 0x03, 0x60, 0x06,
  0x03, 0x60, 0x06, 0x03, 0x60, 0x06, 0x03, 0xe0, 0x06, 0x03, 0xe0, 0x07,
  0x03, 0x60, 0x07, 0xff, 0x7f, 0x06, 0xfe, 0x3f, 0x00
};

#define DISPLAY_WIDTH u8g2->getDisplayWidth()
#define DISPLAY_HEIGHT u8g2->getDisplayHeight()

#define ALIGN_CENTER(x, t) ((x - (u8g2->getUTF8Width(t))) / 2)
#define ALIGN_RIGHT(x, t) (x - u8g2->getUTF8Width(t))
#define ALIGN_LEFT 0
#define drawStrCenter(x, y, t) drawStr(x - u8g2->getUTF8Width(t) / 2, y, t)

#define PT_FONT_10 u8g2_font_6x13_tf  // u8g2_font_7x14_tf
#define PT_FONT_16 u8g2_font_crox5t_tr

void drawCenter(String v, int x, boolean bold = false) {
  x = DISPLAY_WIDTH * x / AnalogMax;
  u8g2->drawLine(x, 28, x, 32);
  if (bold) {
    u8g2->setFont(PT_FONT_16);
  } else {
    u8g2->setFont(PT_FONT_10);
  }
  u8g2->drawStrCenter(x, 41, v.c_str());
}

void displaySetup() {
  //U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 16, /* data=*/ 17);
  // Clock (SCL) and Data (SDA)
  // HELTEC u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, 16, 15, 4);
  // DOIT u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, 22, 21);
  // The larger white displays are: U8G2_SH1106_128X64_NONAME_F_HW_I2C
  u8g2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, PIN_RESET, PIN_CLOCK, PIN_DATA);

  u8g2->begin();
}

void displayLoop(int pan, int tilt, int zoom, int panSpeed, int tiltSpeed, int zoomSpeed) {
  // PSK TODO BUG
  // If last action > 10 minutes turn display off or just don't do anything after clear
  u8g2->clearBuffer();

  u8g2->setFont(PT_FONT_10);
  //u8g2->setTextAlignment(TEXT_ALIGN_LEFT);

  String l1, l2;
  if (WiFi.status() == WL_CONNECTED) {
    u8g2->drawXBM(0, 2, wifi_width, wifi_height, wifi_bits);
    l1 = getHostname();
    l2 = localIP().toString();
  } else if (ETH.linkUp()) {
    // Chagne to ethernet Icon
    l1 = getHostname();
    l2 = localIP().toString();
  } else if (WiFi.getMode() == WIFI_AP) {  // only if in sta mode
    u8g2->drawXBM(32, 0, ptz_setup_width, ptz_setup_height, ptz_setup_bits);
    u8g2->sendBuffer();
    return;

    // Change to AP sign
    l1 = ssid;
    l2 = WiFi.softAPIP().toString();
  }
  u8g2->drawStrCenter(51, 10, l1.c_str());
  u8g2->drawStrCenter(51, 20, l2.c_str());

  if (atemSwitcher.isConnected()) {
    u8g2->drawXBM(85, 2, atem_width, atem_height, atem_bits);
  }

  u8g2->drawXBM(109, 2, cam_width, cam_height, cam_bits);
  u8g2->setFont(PT_FONT_10);
  u8g2->drawStr(114, 16, String(1 + getActiveCamera()).c_str());

  u8g2->setFont(PT_FONT_10);

  if (1) {  //WiFi.status() == WL_CONNECTED || ETH.linkUp()) {
    u8g2->drawStr(0, 52, CommandBuffer[1].c_str());
    u8g2->drawStr(0, 63, CommandBuffer[2].c_str());
  } else if (WiFi.getMode() == WIFI_AP) {  // only if in sta mode
    u8g2->drawStr(0, 52, "1- Connect WiFi to SSID");
    u8g2->drawStr(0, 63, "2- Open browser to IP");
  }

  //int deadZone = DISPLAY_WIDTH * THRESHOLD;
  //Serial.printf("deadZone: %d %d %d\n", deadZone, DISPLAY_WIDTH/2 - deadZone/2, DISPLAY_WIDTH/2 + deadZone/2 );

  u8g2->drawLine(0, 25, DISPLAY_WIDTH, 25);
  //u8g2->drawFrame( DISPLAY_WIDTH / 2 - deadZone / 2, 24, deadZone, 3 );

  if (!settings.hideJoystickPosition) {
    drawCenter("P", pan, panSpeed != 0);
    drawCenter("T", tilt, tiltSpeed != 0);
    drawCenter("Z", zoom, zoomSpeed != 0);
  }
  // Display it on the screen
  u8g2->sendBuffer();
}

void drawBoolean(int x, int y, int sz, boolean value) {
  if (value) {
    u8g2->drawBox(x, y, sz, sz);
  } else {
    u8g2->drawFrame(x, y, sz, sz);
  }
}

void drawCommand(String command) {
  logi(command.c_str());
  logi("\n");
  CommandBuffer[0] = CommandBuffer[1];
  CommandBuffer[1] = CommandBuffer[2];
  CommandBuffer[2] = command;
}

void displayCalibrateScreen(String direction, int pct, int p, int t, int z) {
  u8g2->clearBuffer();
  u8g2->setFont(PT_FONT_16);
  //u8g2->setTextAlignment(TEXT_ALIGN_CENTER);
  u8g2->drawStrCenter(DISPLAY_WIDTH / 2, 16, direction.c_str());

  u8g2->drawLine(0, 30, DISPLAY_WIDTH, 30);

  u8g2->drawLine(DISPLAY_WIDTH / 2, 24, DISPLAY_WIDTH / 2, 30);
  Serial.println("5");
  ;
  drawCenter("P", p);
  Serial.println("6");
  drawCenter("T", t);
  drawCenter("Z", z);

  // u8g2->drawProgressBar(0, DISPLAY_HEIGHT - 11, 120, 10, pct);
  u8g2->sendBuffer();
  Serial.println("7");
}

void drawButton1() {
  u8g2->drawDisc(DISPLAY_WIDTH - 15, 31, 3);
  u8g2->sendBuffer();
}

void drawButton2() {
  u8g2->drawDisc(DISPLAY_WIDTH - 5, 31, 3);
  u8g2->sendBuffer();
}
