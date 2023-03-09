#pragma once
#include <Arduino.h>
#include <ATEMmin.h>
//#include <ESPAsyncWebServer.h>
#include <WebServer.h>
#include <WebSocketServer.h>

using namespace net;
extern WebSocketServer webSocketServer;


#define logi(format, ...) display_i(format, ##__VA_ARGS__); log_i(format, ##__VA_ARGS__);

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

#define AP_SSID "ptz-setup"

#define REV_MODELS 4

#define NETWORK_ERROR 0
#define NETWORK_SUCCESS 1
#define NETWORK_TIMEOUT -1
#define VISCA_PORT            52381 // or the PTZ / HuddleCam port of 1259 w/o headers
#define CAMERA_UP 1
#define CAMERA_DOWN 2
#define CAMERA_OFF 3
#define CAMERA_NA 4


#define BOARD_NAME Pinouts[HWRev].name       // Board
#define PIN_TILT Pinouts[HWRev].tilt         // Yellow
#define PIN_PAN Pinouts[HWRev].pan           // Green
#define PIN_ZOOM Pinouts[HWRev].zoom         // Blue
#define PIN_OVERRIDE Pinouts[HWRev].oride    // Yellow
#define PIN_RECALL_1 Pinouts[HWRev].recall1  // Green
#define PIN_RECALL_2 Pinouts[HWRev].recall2  // Blue
// #define PIN_TRANSMIT Pinouts[HWRev].led      // Internal
#define PIN_RESET Pinouts[HWRev].reset  // Internal or Unused
#define PIN_CLOCK Pinouts[HWRev].clock  // Yellow
#define PIN_DATA Pinouts[HWRev].data    // Green


struct Pinouts_S {
  char name[20];
  int tilt;     // Yellow
  int pan;      // Green
  int zoom;     // Blue
  int oride;    // Yellow
  int recall1;  // Green
  int recall2;  // Blue
  int led;      // Internal
  int reset;    // u8g2 lcd reset
  int clock;    // u8g2 lcd clock
  int data;     // u8g2 lcd data
};

//Define sturct for holding PTZ settings (mostly to simplify EEPROM read and write, in order to persist settings)
struct Settings {
  char ptzName[32];
  bool staticIP;
  IPAddress staticIPAddr;
  IPAddress staticSubnetMask;
  IPAddress staticGateway;
  IPAddress switcherIP;

  // These values are joystick dependent and used to store the auto calibration
  uint16_t panMin;
  uint16_t panMid;
  uint16_t panMax;
  uint16_t tiltMin;
  uint16_t tiltMid;
  uint16_t tiltMax;
  uint16_t zoomMin;
  uint16_t zoomMid;
  uint16_t zoomMax;
  bool hideJoystickPosition;

// Cameras + 1 below for hidden broadcast camera
#define NUM_CAMERAS 20
#define CAMERA_BROADCAST NUM_CAMERAS
#define PROTOCOL_UDP 0
#define PROTOCOL_TCP 1
  IPAddress cameraIP[NUM_CAMERAS + 1];
  uint8_t cameraProtocol[NUM_CAMERAS + 1];
  uint16_t cameraPort[NUM_CAMERAS + 1];
  uint8_t cameraHeaders[NUM_CAMERAS + 1];
};

struct LogItem {
  int type;
  int time;
  char buf[256];
};

extern bool FirstTimeSetup;
extern int HWRev;
extern int AnalogMax;
extern ATEMmin atemSwitcher;
extern char ssid[];
extern WiFiUDP udp;
extern bool InSimulator;
extern WebServer srvr;
extern struct Pinouts_S Pinouts[];
extern struct Settings settings;
extern struct LogItem LogItems[];

extern const char config_html PROGMEM[];
extern const char restart_html PROGMEM[];
extern const char bootstrap_bundle_min_js PROGMEM[];
extern const char bootstrap_min_css PROGMEM[];
extern const char headers_css PROGMEM[];
extern const char validate_forms_js PROGMEM[];

// Extern functions
/*
static void handleLog(AsyncWebServerRequest *request);
*/
void handleLogData();
struct LogItem getLogItem(uint8_t i);
void cameraControlLoop();
void cameraControlSetup();
void calibrateCenter();
void autoCalibrate();
void displayCalibrateScreen(String direction, int pct, int p, int t, int z);
void displayLoop(int pan, int tilt, int zoom, int panSpeed, int tiltSpeed, int zoomSpeed);
void displaySetup();
void drawButton1();
void drawButton2();
int getActiveCamera();
bool ethUp();
bool wifiUp();
bool hotspotUp();
String getSSID();
bool networkUp();
const char* getHostname();
IPAddress localIP();
IPAddress subnetMask();
IPAddress gatewayIP();
//void logi_s(String s);
//void logi(const char *fmt, ...);
void display_i(const char *fmt, ...);
void logd(const char *fmt, ...);
void loge(const char *fmt, ...);
void logw(const char *fmt, ...);
void ptzDrive(int, int, int);
void setupDefaults();
String stringBytes(byte array[], unsigned int len);
void printBytes(byte array[], unsigned int len);
void writeBytes( uint32_t value, byte packet[], int position );
void viscaSetup();
void visca_recall(int);
int cameraStatus(int);
void webSetup();
void webLoop();
int discoverCameras();
String discoveredCameraName(int i);
IPAddress discoveredCameraIP(int i);


int connect( int cameraNumber );
int send( int cameraNumber, byte packet[], int size );
int recieve( int cameraNumber, byte packet[] );

