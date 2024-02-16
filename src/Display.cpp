// https://github.com/ThingPulse/esp8266-oled-ssd1306

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include <Wire.h>
#include <WiFi.h>
#include <ETH.h>

#include "globals.h"
bool DebugDisplay = false;

#define wifi_width 18
#define wifi_height 17
const uint8_t wifi_bits[] = {
  0x00, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0xf8, 0x7f, 0x00, 0x1e, 0xe0, 0x01,
  0x06, 0x80, 0x01, 0xc0, 0x0f, 0x00, 0xf0, 0x3f, 0x00, 0x3c, 0xf0, 0x00,
  0x0c, 0xc0, 0x00, 0x80, 0x07, 0x00, 0xe0, 0x1f, 0x00, 0x78, 0x78, 0x00,
  0x18, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00
};

#define eth_width 18
#define eth_height 17
static unsigned char eth_bits[] = {
   0x27, 0xc9, 0x01, 0x27, 0xc9, 0x01, 0x27, 0xc9, 0x01, 0x27, 0xc9, 0x01,
   0xff, 0xff, 0x01, 0xff, 0xff, 0x01, 0xff, 0xff, 0x01, 0xff, 0xff, 0x01,
   0xff, 0xff, 0x01, 0xff, 0xff, 0x01, 0xff, 0xff, 0x01, 0xff, 0xff, 0x01,
   0xff, 0xff, 0x01, 0xf0, 0x1f, 0x00, 0xf0, 0x1f, 0x00, 0xf0, 0x1f, 0x00,
   0xf0, 0x1f, 0x00 };

#define ap_width 18
#define ap_height 17
static unsigned char ap_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x0e, 0xc0, 0x01,
   0x06, 0x80, 0x01, 0xc7, 0x8c, 0x03, 0xc7, 0x8c, 0x03, 0x63, 0x18, 0x03,
   0x63, 0x18, 0x03, 0x63, 0x18, 0x03, 0xc7, 0x8c, 0x03, 0xc6, 0x8c, 0x03,
   0x06, 0x80, 0x01, 0x0e, 0xc0, 0x01, 0x0c, 0xc0, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 };

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

#define TFT_CS    14 // CS Grey 
#define TFT_RST   4 // RES Dark Brown 
#define TFT_DC    15 // RS Blue

#define TFT_SCLK 16   // SCK White -- Baord says 18
#define TFT_MOSI 13  // SDA Light Brown -- Board says 23

#define TFT_BLK 2 // Backlit - Purple


#define GFX_BL TFT_BLK // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

/* More dev device declaration: https://github.com/moononournation/Arduino_GFX/wiki/Dev-Device-Declaration */
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
//Arduino_DataBus *bus = create_default_Arduino_DataBus();
Arduino_DataBus *bus = new Arduino_SWSPI(TFT_DC /* DC */, TFT_CS/* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, GFX_NOT_DEFINED /* MISO */);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
// Arduino_GFX *gfx = new Arduino_ILI9341(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);
Arduino_GFX *gfx;


#endif /* !defined(DISPLAY_DEV_KIT) */
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

#define PT_FONT_10 u8g2_font_6x13_tf  // u8g2_font_7x14_tf
#define PT_FONT_16 u8g2_font_logisoso16_tf // u8g2_font_crox5t_tr


void drawStrCenter( String s, int x, int y ) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(s,  x,  y,  &x1,  &y1,  &w,  &h);
    gfx->setCursor(x-w/2, y);
    gfx->print(s);
}

void drawPTZ(String v, int x, boolean bold = false) {
  x = gfx->width() * x / AnalogMax;
  gfx->drawLine(x, 28, x, 32, WHITE);
  if ( bold ) {
    gfx->setFont(PT_FONT_16);                  //????????????????????
  } else {
    gfx->setFont(PT_FONT_10);
  }
  // PSK was 41
  drawStrCenter(v, x, 43+((bold==true)?8:0));
}

void displaySetup() {
  #ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
  #endif

  if ( !InSimulator ) {
    Arduino_GFX *gfx_chip = new Arduino_ST7735(
  bus, TFT_RST /* RST */, 1 /* rotation */, false /* IPS */,
  128 /* width */, 160 /* height */,
  0 /* col offset 1 */, 0 /* row offset 1 */,
  0 /* col offset 2 */, 0 /* row offset 2 */,
  false /* BGR */);
  gfx = new Arduino_Canvas(160 /* width */, 128 /* height */, gfx_chip);

  } else {
    gfx = new Arduino_ILI9341(bus, TFT_RST, 1, false);
  }

  // Init Display
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->setTextWrap(false);
  gfx->fillScreen(BLACK);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  gfx->setCursor(10, 10);
  gfx->setTextColor(YELLOW);
  gfx->println("Hello World!");

  delay(1000);
  gfx->fillScreen(BLACK);

  // If Override is pressed on startup (here) go into debug mode on the display
  if( overridePreview() ) {
    DebugDisplay = true;
  }
}

void displayLoop(int pan, int tilt, int zoom, int panSpeed, int tiltSpeed, int zoomSpeed) {
  // TODO BUG Prevent display burn in
  // If last action > 10 minutes turn display off or just don't do anything after clear

  gfx->fillScreen(BLACK);
  gfx->setFont(PT_FONT_10);


  //Prints log to display
  if ( DebugDisplay ) {
    gfx->setCursor(0,8);
    gfx->println(getLogItem(5).buf);
    gfx->setCursor(0,19);
    gfx->println(getLogItem(4).buf);
    gfx->setCursor(0,30);
    gfx->println(getLogItem(3).buf);
    gfx->setCursor(0,41);
    gfx->println(getLogItem(2).buf);
    gfx->setCursor(0,52);
    gfx->println(getLogItem(1).buf);
    gfx->setCursor(0,63);
    gfx->println(getLogItem(0).buf);
    gfx->flush();
    return;
  }

  String l1, l2;
  if ( wifiUp() ) {
    gfx->drawXBitmap(4, 4, wifi_bits, wifi_width, wifi_height, WHITE);
    l1 = String(getHostname());
    l2 = localIP().toString();
  } else if ( ethUp() ) {
    gfx->drawXBitmap(4, 4, eth_bits, eth_width, eth_height, WHITE);
    l1 = String(getHostname());
    l2 = localIP().toString();
  } else if ( hotspotUp() ) {  // only if in AP mode
    gfx->drawXBitmap(4, 4, ap_bits, ap_width, ap_height, WHITE);
    l1 = AP_SSID;
    l2 = WiFi.softAPIP().toString();
  }

  drawStrCenter(l1, 64, 10 );
  drawStrCenter(l2, 64, 21 );   

  if ( atemSwitcher.isConnected() ) {
    gfx->drawXBitmap(137, 3, atem_bits, atem_width, atem_height, WHITE);
  }

  if ( networkUp() ) {
    // Not the prettiest, but need to nudge the camera number over if its double digits
    // and need to not display it at all if its 3 digits
    gfx->drawXBitmap(4, 105, cam_bits, cam_width, cam_height,((overridePreview())?RED:WHITE));
    
    int cn = getActiveCamera();
    int x = 5;
    if ( getActiveCamera() + 1 > 9 ) x -= 3;
    String cns = "";
    if ( cn < 100 ) cns = String(1 + getActiveCamera());
    gfx->setCursor(9,120);
    gfx->println(cns.c_str());
    gfx->setCursor(30,107);
    gfx->println(atemSwitcher.getInputShortName(getActiveCamera() + 1));
  } else {
    gfx->setCursor(0, 52);
    gfx->println("1- Connect WiFi to " AP_SSID);
    gfx->setCursor(0, 63);
    gfx->println("2- Open browser to IP above");
  }

  gfx->drawFastHLine(0, 25, gfx->width(), WHITE);

  if ( !settings.hideJoystickPosition ) {
    drawPTZ("P", pan, panSpeed != 0);
    drawPTZ("T", tilt, tiltSpeed != 0); 
    drawPTZ("Z", zoom, zoomSpeed != 0);
  }

  // Flush the frame buffer in the Arduino_Canvas to the TFT
  gfx->flush();
}

/*
void drawBoolean(int x, int y, int sz, boolean value) {
  if ( value ) {
    u8g2->drawBox(x, y, sz, sz);
  } else {
    u8g2->drawFrame(x, y, sz, sz); //????????????????
  }
}
*/

void drawButton1() {
  gfx->drawCircle(100, 110, 3, RED);
  gfx->flush();
}

void drawButton2() {
  gfx->drawCircle(110, 110, 3, RED);
  gfx->flush();
}
