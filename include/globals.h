#pragma once
#include <Arduino.h>
#include <ATEMmin.h>

// Web UI + telemetry WebSocket are served by PsychicHttp (one async server on
// port 80). The concrete PsychicHttp types stay inside Web.cpp; other modules
// push telemetry through broadcastTelemetry() below.


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

// Board pinouts: the real hardware (Olimex ESP32-PoE) and the Wokwi simulator.
// Selected directly from InSimulator -- no auto-detection.
#define REV_MODELS 2
#define REV_OLIMEX 0
#define REV_SIM    1

#define NETWORK_ERROR 0
#define NETWORK_SUCCESS 1
#define NETWORK_TIMEOUT -1
#define VISCA_PORT 52381 // or the PTZ / HuddleCam port of 1259 w/o headers
#define VISCA_RESPONSE_SIZE 256 // size contract for VISCA response buffers
#define CAMERA_UP 1
#define CAMERA_DOWN 2
#define CAMERA_OFF 3
#define CAMERA_NA 4

// Camera command dialect and network transport are two independent settings.
#define CAM_VISCA 0
#define CAM_ONVIF 1
#define CAM_UDP   0
#define CAM_TCP   1

// EEPROM settings header. Bump SETTINGS_VERSION on any Settings layout change so
// old/blank EEPROM is detected and reset to defaults instead of read as garbage.
#define SETTINGS_MAGIC   0x43436D31u  // "CCm1"
#define SETTINGS_VERSION 3            // v2: NUM_CAMERAS 20->8; v3: per-camera ONVIF user/pass

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
  int pan;      // Green
  int tilt;     // Yellow
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
  uint32_t magic;    // SETTINGS_MAGIC + SETTINGS_VERSION validate the EEPROM blob
  uint16_t version;

  char ssid[32];
  char psk[32];

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
  bool hideJoystickPosition2;

// Cameras + 1 below for hidden broadcast camera. 8 is the design ceiling (the
// largest ATEM in this class has 8 inputs, and the on-device camera strip lays
// out exactly 8 tiles); bump SETTINGS_VERSION if this ever changes.
#define NUM_CAMERAS 8
#define CAMERA_BROADCAST NUM_CAMERAS
  IPAddress cameraIP[NUM_CAMERAS + 1];
  uint8_t cameraType[NUM_CAMERAS + 1];       // CAM_VISCA | CAM_ONVIF
  uint8_t cameraTransport[NUM_CAMERAS + 1];  // CAM_UDP   | CAM_TCP
  uint16_t cameraPort[NUM_CAMERAS + 1];
  uint8_t cameraHeaders[NUM_CAMERAS + 1];
  char cameraUser[NUM_CAMERAS + 1][24];      // ONVIF WS-Security username (VISCA ignores)
  char cameraPass[NUM_CAMERAS + 1][24];      // ONVIF WS-Security password (VISCA ignores)
};

struct LogItem {
  int type;
  uint32_t time;
  char buf[256];
};

// One tick of joystick input, bundled so the control loop and the display share
// a single value instead of threading six ints through every call. Kept as a
// passed-by-const-ref parameter (not a global) so it stays confined to the
// main-loop task and can't be torn by the async web task.
struct JoystickState {
  int pan, tilt, zoom;                 // raw analogRead positions (0..AnalogMax)
  int panSpeed, tiltSpeed, zoomSpeed;  // mapped VISCA drive speeds (signed)
};

// A camera found by auto-discovery (VISCA broadcast or ONVIF WS-Discovery).
// Cameras are identified by ip+port, so multiple cameras may share one IP.
struct DiscoveredCamera {
  IPAddress ip;
  uint16_t  port;
  uint8_t   type;       // CAM_VISCA | CAM_ONVIF
  uint8_t   transport;  // CAM_UDP   | CAM_TCP
  char      name[24];
};
#define MAX_DISCOVERED 16

extern bool FirstTimeSetup;
extern int HWRev;
extern int AnalogMax;
extern ATEMmin atemSwitcher;
extern char ssid[];
extern bool InSimulator;
extern struct Pinouts_S Pinouts[];
extern struct Settings settings;
extern struct LogItem LogItems[];

// Web assets (config_html, restart_html, gzipped css/js) are embedded in flash
// via board_build.embed_* and declared as asm-aliased symbols in Web.cpp.

// Extern functions
void broadcastTelemetry(const char* msg);   // push a telemetry frame to /ws clients (Web.cpp)
extern volatile uint32_t g_txCount;          // packets sent (display TX histogram)
extern bool WiFiWorked;                      // WiFi has connected at least once (connecting vs reconnecting)
void notePreset(int num, bool isSet);        // latch a preset recall/save for the display's button pill
extern volatile bool g_otaActive;            // firmware update in progress (display shows the OTA screen)
extern volatile uint32_t g_otaBytes;         // bytes written so far during OTA
extern volatile uint32_t g_otaLastChunk;     // millis() of the last received OTA chunk (stuck detection)
struct LogItem getLogItem(uint8_t i);
void cameraControlLoop();
void cameraControlSetup();
void calibrateCenter();
void autoCalibrate();
void displayCalibrateScreen(String direction, int pct, int p, int t, int z);
void displayLoop(const JoystickState& js);
void displaySetup();
void drawButton1();
void drawButton2();
int getActiveCamera();
bool ethUp();
bool wifiUp();
bool hotspotUp();
String getSSID();
String getPSK();
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
void visca_recall_memory(int);
void visca_set_memory(int);
int cameraStatus(int);        // blocking probe -- MAIN-LOOP TASK ONLY (does socket I/O)
void pollCameraStatus();      // main loop: refresh one camera's cached status per tick
int cachedCameraStatus(int);  // web handler: read cached status (no I/O, race-free)
int viscaStatus(int);    // VISCA power inquiry (Visca.cpp)
int onvifStatus(int);    // ONVIF TCP reachability (Onvif.cpp)
void webSetup();
void webLoop();
extern DiscoveredCamera discoveredCameras[MAX_DISCOVERED];
extern int numDiscoveredCameras;
void resetDiscovered();
bool addDiscovered(IPAddress ip, uint16_t port, uint8_t type, uint8_t transport, const char *name);
int discoverCameras();        // VISCA broadcast probe (appends to discoveredCameras)
int discoverOnvifCameras();   // ONVIF WS-Discovery (appends to discoveredCameras)
boolean overridePreview();

void networkSetup();
void networkServicesLoop();

int camConnect( int cameraNumber );
int camSend( int cameraNumber, byte packet[], int size );
int camRecv( int cameraNumber, byte packet[], size_t cap );
void camClose( int cameraNumber );
IPAddress camRemoteIP( int cameraNumber );
uint16_t camRemotePort( int cameraNumber );

void Onvif_SetPreset(int presetNumber);
void Onvif_GoToPreset(int presetNumber);
void Onvif_PtzDrive(int, int, int);
void Onvif_PanTiltDrive(int panSpeed, int tiltSpeed);
void Onvif_ZoomDrive(int zoomSpeed);
void Onvif_Stop(bool stopPanTilt, bool stopZoom);

