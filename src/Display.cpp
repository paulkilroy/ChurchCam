// https://github.com/ThingPulse/esp8266-oled-ssd1306
//hello test
//this is a big test to see is i can push code to gihub
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include <Wire.h>
#include <WiFi.h>
#include <ETH.h>
#include <math.h>

#include "globals.h"

// Arduino_GFX 1.6.x renamed the basic colour macros to RGB565_*; keep the
// short names this file uses.
#define BLACK  RGB565_BLACK
#define WHITE  RGB565_WHITE
#define RED    RGB565_RED
#define GREEN  RGB565_GREEN
#define YELLOW RGB565_YELLOW

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

#define TFT_SCLK 5   // SCK White -- Baord says 18
#define TFT_MOSI 13  // SDA Light Brown -- Board says 23

#define TFT_BLK 2 // Backlit - Purple


#define GFX_BL TFT_BLK // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

/* More dev device declaration: https://github.com/moononournation/Arduino_GFX/wiki/Dev-Device-Declaration */
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
//Arduino_DataBus *bus = create_default_Arduino_DataBus();
//Arduino_DataBus *bus = new Arduino_SWSPI(TFT_DC /* DC */, TFT_CS/* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC /* DC */, TFT_CS/* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, GFX_NOT_DEFINED /* MISO */);
/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
// Arduino_GFX *gfx = new Arduino_ILI9341(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);
Arduino_GFX *gfx;

double pi = PI;
#define arcRadius 80
double end = 180;

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

void drawP(int x, boolean bold = false) {
  
  x = 160.0+(((double)x/(double)AnalogMax)*120.0)-60.0;
  
  
  if ( bold ) {
    gfx->setFont(PT_FONT_16);                  //????????????????????
  } else {
    gfx->setFont(PT_FONT_10);
  }

  drawStrCenter("P", x, 140);
}
void drawT(int y, boolean bold = false) {
  
  y = 140.0+(((double)y/(double)AnalogMax)*120.0)-60.0;
  
  
  if ( bold ) {
    gfx->setFont(PT_FONT_16);                  //????????????????????
  } else {
    gfx->setFont(PT_FONT_10);
  }

  drawStrCenter("T", 160, y);
}

double zoomRadians(){
  return (((analogRead(PIN_ZOOM))/4059.0*180.0)+90.0)*(PI/180);
}

int findArc_X(int angle){
    return arcRadius*sin(-(zoomRadians())) + 160;
    
}

int findArc_Y(int angle){
    return arcRadius*cos(zoomRadians()) + 140;
}

void displaySetup() {
  #ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
  #endif

  // Same double-buffered canvas on hardware and in the simulator: displayLoop()
  // draws into the Arduino_Canvas_Indexed framebuffer and flush()es one frame to
  // the panel. A direct (un-buffered) panel here flickers, because every frame
  // starts with fillScreen(BLACK) on the live display.
  Arduino_GFX *gfx_chip = new Arduino_ILI9341(bus, TFT_RST /* RST */, 3 /* rotation */, false /* IPS */);
  gfx = new Arduino_Canvas_Indexed(320 /* width */, 240 /* height */, gfx_chip, 0, 0, 0);

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

// ===========================================================================
// Operator console layout (320x240): tally-framed camera, pan/tilt radar +
// zoom rail, an 8-input camera-bus strip, and a TX/sec histogram. All drawn
// with flat fills / lines / u8g2 fonts into the double-buffered canvas.
// ===========================================================================

// RGB565 helper + palette (kept small for the indexed canvas).
#define C565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
#define COL_AMBER  C565(255,190,20)
#define COL_GRAY   C565(96,104,120)
#define COL_DIM    C565(52,60,74)
#define COL_TRACK  C565(26,30,40)
#define COL_GREEN2 C565(22,120,54)
#define COL_RULE   C565(110,120,134)

#define FONT_TINY u8g2_font_5x7_tf
#define FONT_SM   PT_FONT_10                 // 6x13
#define FONT_MD   PT_FONT_16                 // logisoso16
#define FONT_BIG  u8g2_font_logisoso62_tn    // big camera digit (numbers only)

// TX/sec histogram: sample the global send counter once per second into a ring.
extern volatile uint32_t g_txCount;
#define HIST_BARS 48
#define HIST_MAX  30
static uint8_t  txHist[HIST_BARS];
static uint32_t txLastSample = 0, txLastCount = 0;
static void sampleTx() {
  uint32_t now = millis();
  if ( now - txLastSample < 1000 ) return;
  txLastSample = now;
  uint32_t c = g_txCount, d = c - txLastCount;
  txLastCount = c;
  if ( d > HIST_MAX ) d = HIST_MAX;
  for ( int i = 0; i < HIST_BARS - 1; i++ ) txHist[i] = txHist[i + 1];
  txHist[HIST_BARS - 1] = (uint8_t)d;
}

// Small text helpers (u8g2 fonts draw from the baseline y).
static void txt(const uint8_t* f, uint16_t c, int x, int y, const char* s) {
  gfx->setFont(f); gfx->setTextColor(c); gfx->setCursor(x, y); gfx->print(s);
}
static void txtC(const uint8_t* f, uint16_t c, int cx, int y, const char* s) {
  gfx->setFont(f); gfx->setTextColor(c);
  int16_t x1, y1; uint16_t w, h; gfx->getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  gfx->setCursor(cx - w / 2, y); gfx->print(s);
}
// Left-aligned at x, but vertically centered inside a box [py, py+ph] -- u8g2
// fonts draw from the baseline, so this solves "text sits too high in the pill".
static void txtVC(const uint8_t* f, uint16_t c, int x, int py, int ph, const char* s) {
  gfx->setFont(f); gfx->setTextColor(c);
  int16_t x1, y1; uint16_t w, h; gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(x, py + (ph - h) / 2 - y1);
  gfx->print(s);
}
// Centered both ways inside a box -- for button pills.
static void txtBoxC(const uint8_t* f, uint16_t c, int bx, int by, int bw, int bh, const char* s) {
  gfx->setFont(f); gfx->setTextColor(c);
  int16_t x1, y1; uint16_t w, h; gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(bx + (bw - w) / 2 - x1, by + (bh - h) / 2 - y1);
  gfx->print(s);
}
static void frameRR(int x, int y, int w, int h, int r, uint16_t c, int t) {
  for ( int i = 0; i < t; i++ ) gfx->drawRoundRect(x + i, y + i, w - 2 * i, h - 2 * i, r > i ? r - i : 0, c);
}

// Signal strength: WiFi RSSI as 0-4 bars; Ethernet shows the wired glyph.
static void drawSignal(int x, int y) {
  if ( ethUp() ) { gfx->drawXBitmap(x, y, eth_bits, eth_width, eth_height, WHITE); return; }
  int rssi = WiFi.RSSI();
  int lvl = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -72 ? 2 : rssi >= -82 ? 1 : 0;
  for ( int i = 0; i < 4; i++ ) {
    int bh = 3 + i * 3;
    gfx->fillRect(x + i * 4, y + 14 - bh, 3, bh, i < lvl ? WHITE : COL_DIM);
  }
}

// Button-press state, latched by notePreset() from the button loop.
// A tap recalls (brief header pill); OVERRIDE+tap saves (full-screen popup).
static uint32_t presetAt = 0;       // recall -> header pill flash
static int presetNum = 0;
static uint32_t presetSaveAt = 0;   // save   -> full-screen confirmation popup
static int presetSaveNum = 0;
void notePreset(int num, bool isSet) {
  if ( isSet ) { presetSaveAt = millis(); presetSaveNum = num; }
  else         { presetAt = millis();     presetNum = num; }
}

static void drawHeader(int active) {
  bool eth = ethUp();
  int hx = eth ? 26 : 24;
  drawSignal(4, 4);
  txt(FONT_TINY, WHITE, hx, 8, getHostname());
  char line2[44];
  if ( eth ) snprintf(line2, sizeof(line2), "%s", localIP().toString().c_str());
  else snprintf(line2, sizeof(line2), "%s  %ddB", localIP().toString().c_str(), (int)WiFi.RSSI());
  txt(FONT_TINY, COL_GRAY, hx, 17, line2);

  // Right slot: a pressed button takes over the slot; otherwise the switcher status.
  bool ovr = overridePreview();
  bool preset = presetAt != 0 && ( millis() - presetAt ) < 1200;
  if ( ovr ) {
    char b[20]; snprintf(b, sizeof(b), "OVERRIDE %d", active + 1);
    gfx->fillRoundRect(150, 2, 164, 21, 3, RED);
    txtBoxC(FONT_MD, WHITE, 150, 2, 164, 21, b);
  } else if ( preset ) {
    char b[20]; snprintf(b, sizeof(b), "PRESET %d RECALL", presetNum);
    gfx->fillRoundRect(150, 2, 164, 21, 3, GREEN);
    txtBoxC(FONT_MD, BLACK, 150, 2, 164, 21, b);
  } else if ( !atemSwitcher.isConnected() ) {
    gfx->drawRoundRect(150, 3, 74, 18, 3, COL_AMBER); txtVC(FONT_TINY, COL_AMBER, 162, 3, 18, "NO ATEM");
  } else {
    gfx->drawRoundRect(170, 4, 44, 16, 3, GREEN); gfx->fillCircle(178, 12, 2, GREEN);
    txtVC(FONT_TINY, GREEN, 187, 4, 16, "ATEM");
    if ( atemSwitcher.getStreamStreaming() ) {
      gfx->fillRoundRect(218, 2, 96, 21, 3, RED); gfx->fillCircle(230, 12, 3, WHITE);
      txtVC(FONT_MD, WHITE, 246, 2, 21, "ON AIR");
    } else {
      gfx->drawRoundRect(218, 2, 96, 21, 3, COL_DIM); txtVC(FONT_MD, COL_GRAY, 242, 2, 21, "OFF AIR");
    }
  }
  gfx->drawFastHLine(0, 25, 320, COL_RULE);
}

// tally: 0=off, 1=preview, 2=program
static void drawTallyBox(int active, int tally) {
  int x = 6, y = 27, w = 120, h = 116;

  // Active input has no camera configured (e.g. ATEM on a laptop/media input).
  if ( settings.cameraIP[active][0] == 0 ) {
    frameRR(x, y, w, h, 5, COL_DIM, 3);
    char t[12]; snprintf(t, sizeof(t), "INPUT %d", active + 1);
    gfx->fillRect(x + 2, y + 2, 66, 13, COL_DIM); txt(FONT_TINY, BLACK, x + 6, y + 12, t);
    gfx->fillRect(x + w / 2 - 16, y + 62, 32, 6, COL_DIM);              // a dim dash
    gfx->drawFastHLine(x + 8, y + h - 26, w - 16, C565(40, 46, 58));
    txt(FONT_TINY, COL_DIM, x + 8, y + h - 14, "NO CAMERA HERE");
    txt(FONT_TINY, C565(49, 56, 74), x + 8, y + h - 5, "(media / laptop)");
    return;
  }

  uint16_t col = tally == 2 ? RED : tally == 1 ? GREEN : COL_GRAY;
  const char* lab = tally == 2 ? "PROGRAM" : tally == 1 ? "PREVIEW" : "OFF";
  frameRR(x, y, w, h, 5, col, 3);
  gfx->fillRect(x + 2, y + 2, 58, 13, col);
  txt(FONT_TINY, tally == 1 ? BLACK : WHITE, x + 6, y + 12, lab);

  char num[6]; snprintf(num, sizeof(num), "%d", active + 1);
  gfx->setFont(FONT_BIG); gfx->setTextColor(WHITE);
  int16_t x1, y1; uint16_t nw, nh; gfx->getTextBounds(num, 0, 0, &x1, &y1, &nw, &nh);
  gfx->setCursor(x + w / 2 - nw / 2, y + 90);
  gfx->print(num);

  gfx->drawFastHLine(x + 8, y + h - 26, w - 16, C565(40, 46, 58));
  String name = String(atemSwitcher.getInputShortName(active + 1));
  if ( name.length() == 0 ) name = "CAM " + String(active + 1);
  txt(FONT_TINY, WHITE, x + 8, y + h - 14, name.c_str());
  bool up = cachedCameraStatus(active) == CAMERA_UP;
  const char* proto = settings.cameraType[active] == CAM_ONVIF ? "ONVIF"
                    : ( settings.cameraTransport[active] == CAM_TCP ? "VISCA TCP" : "VISCA UDP" );
  gfx->fillCircle(x + 11, y + h - 8, 2, up ? GREEN : COL_AMBER);
  txt(FONT_TINY, up ? GREEN : COL_AMBER, x + 17, y + h - 5, proto);
}

static void drawRadar(int pan, int tilt, int zoom, int tally) {
  uint16_t col = tally == 2 ? RED : GREEN;
  int cx = 222, cy = 88, r = 56;
  gfx->drawCircle(cx, cy, r, col); gfx->drawCircle(cx, cy, r - 1, col);
  gfx->drawCircle(cx, cy, r / 2, COL_TRACK);
  gfx->drawFastHLine(cx - r, cy, 2 * r, COL_TRACK);
  gfx->drawFastVLine(cx, cy - r, 2 * r, COL_TRACK);
  float pn = (pan  / (float)AnalogMax) * 2.0f - 1.0f;
  float tn = (tilt / (float)AnalogMax) * 2.0f - 1.0f;
  int dx = cx + (int)(pn * (r - 6)), dy = cy - (int)(tn * (r - 6));
  gfx->fillCircle(dx, dy, 5, col); gfx->drawCircle(dx, dy, 6, BLACK);

  int zx = 306, ztop = 32, zh = 110;
  gfx->fillRect(zx + 2, ztop, 3, zh, COL_TRACK);
  float zn = zoom / (float)AnalogMax; if ( zn < 0 ) zn = 0; if ( zn > 1 ) zn = 1;
  int fh = (int)(zn * zh);
  gfx->fillRect(zx, ztop + zh - fh, 7, fh, col);
  txt(FONT_TINY, COL_GRAY, zx, ztop + zh + 9, "Z");
}

static void drawCameraStrip(int active) {
  int pgm = atemSwitcher.isConnected() ? atemSwitcher.getProgramInputVideoSource(0) : -1;
  int pvw = atemSwitcher.isConnected() ? atemSwitcher.getPreviewInputVideoSource(0) : -1;
  txt(FONT_TINY, COL_DIM, 6, 160, "INPUTS");
  int x0 = 6, y = 164, W = 308, h = 36, n = NUM_CAMERAS, gap = 4;   // strip is sized for 8 tiles
  int tw = (W - (n - 1) * gap) / n;
  for ( int i = 0; i < n; i++ ) {
    int input = i + 1, tx = x0 + i * (tw + gap);
    // 0 unset, 1 online, 2 down, 3 preview, 4 program
    int st;
    if ( input == pgm ) st = 4; else if ( input == pvw ) st = 3;
    else { int s = cachedCameraStatus(i); st = s == CAMERA_UP ? 1 : s == CAMERA_DOWN ? 2 : 0; }
    uint16_t col = st == 4 ? RED : st == 3 ? GREEN : st == 2 ? COL_AMBER : st == 1 ? COL_GRAY : COL_DIM;
    frameRR(tx, y, tw, h, 3, col, st >= 3 ? 2 : 1);
    if ( st >= 3 ) gfx->fillRect(tx + 1, y + 1, tw - 2, 4, col);
    char num[4]; snprintf(num, sizeof(num), "%d", input);
    uint16_t nc = st == 0 ? COL_DIM : st == 2 ? COL_AMBER : WHITE;
    txtC(FONT_MD, nc, tx + tw / 2, y + 26, num);
    if ( st == 4 ) txt(FONT_TINY, RED, tx + 3, y + h - 3, "PRG");
    else if ( st == 3 ) txt(FONT_TINY, GREEN, tx + 3, y + h - 3, "PVW");
    else if ( st == 2 ) txt(FONT_TINY, COL_AMBER, tx + tw - 8, y + h - 3, "!");
    if ( input == active + 1 ) gfx->fillRect(tx + tw / 2 - 6, y + h + 3, 12, 2, WHITE);
  }
}

static void drawHistogram() {
  int y = 225, h = 12, x0 = 22, W = 292, gap = 1;
  float bw = (W - (HIST_BARS - 1) * gap) / (float)HIST_BARS;
  gfx->drawFastHLine(0, 220, 320, C565(30, 34, 44));
  txt(FONT_TINY, COL_DIM, 3, 231, "TX");
  for ( int i = 0; i < HIST_BARS; i++ ) {
    int bh = txHist[i] * h / HIST_MAX;
    if ( txHist[i] > 0 && bh < 1 ) bh = 1;
    if ( bh <= 0 ) continue;
    int bx = x0 + (int)(i * (bw + gap));
    gfx->fillRect(bx, y + h - bh, (int)bw + 1, bh, i > HIST_BARS - 6 ? GREEN : COL_GREEN2);
  }
}

// AP / config mode: no station or Ethernet link -- the device is only reachable
// on its own hotspot, so take over the whole screen and direct the operator there.
static void drawConfigScreen() {
  gfx->fillRect(0, 0, 320, 26, COL_AMBER);
  txt(FONT_MD, BLACK, 8, 19, "SETUP MODE");
  gfx->drawXBitmap(292, 4, ap_bits, ap_width, ap_height, BLACK);
  txt(FONT_SM, COL_GRAY, 12, 52, "No network yet -- connect");
  txt(FONT_SM, COL_GRAY, 12, 70, "to this device to set it up:");
  txt(FONT_SM, WHITE, 12, 104, "1. Join Wi-Fi");
  txt(FONT_MD, COL_AMBER, 120, 108, AP_SSID);
  txt(FONT_SM, WHITE, 12, 150, "2. Open a browser to");
  txt(FONT_MD, GREEN, 120, 154, WiFi.softAPIP().toString().c_str());
  gfx->drawFastHLine(0, 182, 320, COL_RULE);
  txt(FONT_TINY, COL_GRAY, 12, 200, "Set Wi-Fi, ATEM & cameras there, then it reconnects.");
}

// Booting or link dropped -- keep trying (the firmware never times out to AP once
// WiFi has worked, so this just says "reconnecting", no fake countdown).
static void drawConnectingScreen() {
  gfx->fillRect(0, 0, 320, 26, C565(23, 18, 5));
  gfx->drawFastHLine(0, 26, 320, COL_AMBER);
  txt(FONT_MD, COL_AMBER, 10, 19, "ChurchCam");
  txt(FONT_TINY, C565(107, 90, 30), 258, 12, "PTZ CTRL");
  bool reconnect = WiFiWorked;
  txtC(FONT_MD, WHITE, 160, 116, reconnect ? "Reconnecting" : "Connecting");
  int n = ( millis() / 400 ) % 4;                     // animated amber dots
  for ( int i = 0; i < 3; i++ ) gfx->fillCircle(140 + i * 20, 144, 4, i < n ? COL_AMBER : C565(58, 47, 12));
  txtC(FONT_TINY, COL_GRAY, 160, 178, reconnect ? "Wi-Fi dropped -- keeps retrying." : "waiting for a network link");
  txtC(FONT_TINY, C565(74, 82, 98), 160, 200, "Restart to change settings.");
}

// OTA in progress: take over the whole screen with progress + a hard warning.
static void drawUpdatingScreen() {
  txtC(FONT_MD, WHITE, 160, 44, "UPDATING FIRMWARE");
  gfx->drawRoundRect(30, 92, 260, 24, 4, C565(42, 51, 64));
  int span = 260 - 60 - 6;                             // indeterminate: bounce a segment
  int pos = ( millis() / 8 ) % ( span * 2 );
  if ( pos > span ) pos = span * 2 - pos;
  gfx->fillRect(33 + pos, 95, 60, 18, COL_AMBER);
  char b[32]; snprintf(b, sizeof(b), "%lu KB written", (unsigned long)( g_otaBytes / 1024 ));
  txtC(FONT_TINY, COL_GRAY, 160, 138, b);
  gfx->fillRect(0, 196, 320, 44, RED);
  gfx->fillCircle(20, 214, 6, WHITE);
  txt(FONT_MD, WHITE, 40, 210, "DO NOT POWER OFF");
  txt(FONT_TINY, WHITE, 40, 226, "the device reboots when done");
}

// One centered P/T/Z bar, drawn dark-on-amber for the save popup.
// Full-screen confirmation after OVERRIDE+tap saves a preset. Shown ~2.5s over
// the console (the control loop keeps running, so the joystick isn't frozen).
// We deliberately do NOT draw the pan/tilt/zoom position here: the joystick is a
// velocity control and the preset is stored on the camera, so the controller has
// no truthful notion of "the saved position" to show.
static void drawPresetSaved() {
  int active = getActiveCamera();
  gfx->fillScreen(COL_AMBER);
  gfx->fillRect(0, 0, 320, 44, BLACK);
  char hdr[24]; snprintf(hdr, sizeof(hdr), "PRESET %d SAVED", presetSaveNum);
  txtVC(FONT_MD, COL_AMBER, 18, 0, 44, hdr);

  String name = String(atemSwitcher.getInputShortName(active + 1));
  if ( name.length() == 0 ) name = "CAM " + String(active + 1);
  char cam[40]; snprintf(cam, sizeof(cam), "CAM %d  %s", active + 1, name.c_str());
  txtVC(FONT_MD, BLACK, 20, 70, 40, cam);

  char hint[48];
  snprintf(hint, sizeof(hint), "Tap button %d to recall this position.", presetSaveNum);
  txt(FONT_TINY, C565(90, 66, 0), 20, 150, hint);
}

void displayLoop(const JoystickState& js) {
  sampleTx();   // internally throttled to 1Hz; keep it at full loop rate

  // Cap the redraw rate. A full fillScreen + flush pushes the whole ~150KB
  // framebuffer over SPI; doing that every control-loop iteration needlessly
  // competes with camera I/O and joystick handling. 30fps is smooth for this UI.
  static uint32_t lastFrame = 0;
  uint32_t nowFrame = millis();
  if ( nowFrame - lastFrame < 33 ) return;
  lastFrame = nowFrame;

  gfx->fillScreen(BLACK);

  // Screen state machine: OTA / config (AP-only) / connecting / console.
  if ( g_otaActive )                          { drawUpdatingScreen();   gfx->flush(); return; }
  if ( hotspotUp() && !wifiUp() && !ethUp() ) { drawConfigScreen();     gfx->flush(); return; }
  if ( !networkUp() )                         { drawConnectingScreen(); gfx->flush(); return; }
  if ( presetSaveAt != 0 && millis() - presetSaveAt < 2500 ) { drawPresetSaved(); gfx->flush(); return; }

  int active = getActiveCamera();
  int input  = active + 1;
  int pgm = atemSwitcher.isConnected() ? atemSwitcher.getProgramInputVideoSource(0) : -1;
  int pvw = atemSwitcher.isConnected() ? atemSwitcher.getPreviewInputVideoSource(0) : -1;
  int tally = ( input == pgm ) ? 2 : ( input == pvw ) ? 1 : 0;

  drawHeader(active);
  drawTallyBox(active, tally);
  drawRadar(js.pan, js.tilt, js.zoom, tally);
  drawCameraStrip(active);
  drawHistogram();

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
  gfx->drawCircle(100, 220, 3, RED);
  gfx->flush();
}

void drawButton2() {
  gfx->drawCircle(110, 220, 3, RED);
  gfx->flush();
}
