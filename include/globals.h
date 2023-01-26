#pragma once
#include <Arduino.h>
#include <ATEMmin.h>
#include <ESPAsyncWebServer.h>

#define REV_MODELS 4

#define NETWORK_ERROR 0
#define NETWORK_SUCCESS 1
#define NETWORK_TIMEOUT -1
#define VISCA_PORT            52381 // or the PTZ / HuddleCam port of 1259 w/o headers


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

extern bool FirstTimeSetup;
extern int HWRev;
extern struct Pinouts_S Pinouts[];
extern int AnalogMax;
extern struct Settings settings;
extern ATEMmin atemSwitcher;
extern char ssid[];
extern WiFiUDP udp;

extern const char config_html PROGMEM[];
extern const char bootstrap_bundle_min_js PROGMEM[];
extern const char bootstrap_min_css PROGMEM[];
extern const char headers_css PROGMEM[];
extern const char validate_forms_js PROGMEM[];



// Extern functions
static void handleLog(AsyncWebServerRequest *request);
static void handleLogData(AsyncWebServerRequest *request);

void cameraControlLoop();
void cameraControlSetup();
void calibrateCenter();
void autoCalibrate();
void displayCalibrateScreen(String direction, int pct, int p, int t, int z);
void displayLoop(int pan, int tilt, int zoom, int panSpeed, int tiltSpeed, int zoomSpeed);
void displaySetup();
void drawCommand(String command);
void drawButton1();
void drawButton2();
int getActiveCamera();
String getSSID();
bool networkUp();
const char* getHostname();
IPAddress localIP();
IPAddress subnetMask();
IPAddress gatewayIP();
static void logi(const char *fmt, ...);
static void logd(const char *fmt, ...);
static void loge(const char *fmt, ...);
static void logw(const char *fmt, ...);
void ptzDrive(int, int, int);
void setupDefaults();
String stringBytes(byte array[], unsigned int len);
void printBytes(byte array[], unsigned int len);
void writeBytes( uint32_t value, byte packet[], int position );
void viscaSetup();
void visca_recall(int);
void webSetup();

int connect( int cameraNumber );
int send( int cameraNumber, byte packet[], int size );
int recieve( int cameraNumber, byte packet[] );

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
#define NUM_CAMERAS 8
#define CAMERA_BROADCAST NUM_CAMERAS
#define PROTOCOL_UDP 0
#define PROTOCOL_TCP 1
  IPAddress cameraIP[NUM_CAMERAS + 1];
  uint8_t cameraProtocol[NUM_CAMERAS + 1];
  uint16_t cameraPort[NUM_CAMERAS + 1];
  uint8_t cameraHeaders[NUM_CAMERAS + 1];
};