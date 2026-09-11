#include <Arduino.h>
#include <EEPROM.h>
#include "globals.h"
#include <time.h>

#define THRESHOLD .1

// If you want to slow down the camera movement just change these from the visca standards below
// 18 is the VISCA max -- too fast

#define ANALOG_RESOLUTION 12
int AnalogMax = pow(2, ANALOG_RESOLUTION) - 1;
#define DEADZONE_SIZE (AnalogMax * THRESHOLD)

// int NoJoystick = 1;  // Just in case
unsigned long LastSendTime = 0;
#define MAX_SEND 100  // Do not send a PTZ message more than every 100ms, unless its a stop

unsigned long startPress = 0;
unsigned long endPress = 0;

#define PAN_SPEED_MAX 15
#define TILT_SPEED_MAX 15
#define ZOOM_SPEED_MAX 7

bool wasButton1Pressed = false;
bool wasButton2Pressed = false;
// OVERRIDE is latched at button-press time, not read at release: the preview
// source (which overridePreview() reflects) can change between press and release,
// and we want the intent the operator had when they started the press.
bool button1Override = false;
bool button2Override = false;

#define LONG_PRESS_TIME 500

// --- Cached status --------------------------------------------------------
// cameraStatus() does blocking socket I/O, so it must run ONLY in the main-loop
// task -- the same task that drives the active camera. The async PsychicHttp
// handler must never call it directly, or two tasks race on the same camera
// socket (which flapped the active camera up/down). Instead the main loop polls
// one camera per tick into this cache and the web UI reads the cache. Seeded to
// CAMERA_NA in cameraControlSetup().
static int      camStatusCache[NUM_CAMERAS];
static int      camPollIdx = 0;
static uint32_t lastStatusPollAt = 0;
#define STATUS_POLL_INTERVAL_MS 1000  // one camera per tick; full 8-slot sweep ~8s

void cameraControlSetup() {
  analogReadResolution(ANALOG_RESOLUTION);  // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
  //analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.

  // Olimex is the only real board; the Wokwi simulator uses its own pinout.
  HWRev = InSimulator ? REV_SIM : REV_OLIMEX;
  logi("Board: [%d] %s", HWRev, Pinouts[HWRev].name);

  pinMode(PIN_PAN, INPUT);
  pinMode(PIN_TILT, INPUT);
  pinMode(PIN_ZOOM, INPUT);
  pinMode(PIN_OVERRIDE, INPUT_PULLUP);
  if (HWRev == REV_OLIMEX) {
    // recall1 + recall2 share GPIO34 as an analog resistor ladder (see the Pinouts
    // note + readRecallButtons()). GPIO34 is input-only; onboard R48 10k is the
    // pull-up, so just INPUT. recall1 has no discrete pin here.
    pinMode(PIN_RECALL_2, INPUT);
  } else {
    // Wokwi sim: discrete digital buttons.
    pinMode(PIN_RECALL_1, INPUT_PULLUP);
    pinMode(PIN_RECALL_2, INPUT_PULLUP);
  }

  // The camStatusCache array initializer only sets element [0]; seed the rest to
  // CAMERA_NA so the web UI never reads a bogus status for an un-polled camera.
  for (int i = 0; i < NUM_CAMERAS; i++) camStatusCache[i] = CAMERA_NA;

  setupDefaults();
  viscaSetup();
  displaySetup();

  // Draw the inital screen once
  JoystickState js = { 0, 0, 0, 0, 0, 0 };
  displayLoop(js);
}

void setupDefaults() {
  if ( FirstTimeSetup ) {
    Serial.printf("AnalogMax = %d\n", AnalogMax);
    settings.panMin = 0;
    settings.panMid = AnalogMax/2;
    settings.panMax = AnalogMax;
    settings.tiltMin = 0;
    settings.tiltMid = AnalogMax/2;
    settings.tiltMax = AnalogMax;
    settings.zoomMin = 0;
    settings.zoomMid = AnalogMax/2;
    settings.zoomMax = AnalogMax;
  }
}

boolean overridePreview() {
  return digitalRead(PIN_OVERRIDE) == LOW ? true : false;
}

// This is where the magic happens. Normally this will talk to the ATEM and find the current preview camera. This is nice
// so no one accidentally moves the camera that is currently live. But if the "override" button is currently pressed (held down)
// then the camera that is on program will be selected for ptz movement or recall
int getActiveCamera() {
  int cam;
  if ( !atemSwitcher.isConnected() ) {
    // default to cam 1
    cam = overridePreview() ? 1 : 0;
  } else if ( overridePreview() ) {
    //Serial.printf("atem: %d - %d\n", atemSwitcher.getPreviewInputVideoSource(0), atemSwitcher.getProgramInputVideoSource(0));
    cam = atemSwitcher.getProgramInputVideoSource(0) - 1;
  } else {
    cam = atemSwitcher.getPreviewInputVideoSource(0) - 1;
  }
  // Clamp to a valid camera index. ATEM source 0 (black/none) yields -1, which
  // would index settings.cameraType[-1]/cameraIP[-1] out of bounds downstream.
  if ( cam < 0 ) cam = 0;
  if ( cam >= NUM_CAMERAS ) cam = NUM_CAMERAS - 1;
  return cam;
}

// VISCA cameras are driven via ptzDrive/visca_send (transport picked per camera
// in CameraLink); ONVIF cameras use the Onvif_* path instead.
static bool isVisca(int cam) {
  return settings.cameraType[cam] == CAM_VISCA;
}

// Protocol-agnostic camera health. The shared reachability checks live here; the
// actual probe is dispatched to the camera's own protocol layer (VISCA power
// inquiry vs. ONVIF TCP reachability) so neither module needs to know the other.
int cameraStatus(int cameraNumber) {
  if (!networkUp() || settings.cameraIP[cameraNumber][0] == 0 ||
      settings.cameraIP[cameraNumber][0] == 255) {
    return CAMERA_NA;
  }
  return isVisca(cameraNumber) ? viscaStatus(cameraNumber)
                               : onvifStatus(cameraNumber);
}

void pollCameraStatus() {
  uint32_t now = millis();
  if (now - lastStatusPollAt < STATUS_POLL_INTERVAL_MS) return;
  lastStatusPollAt = now;
  camStatusCache[camPollIdx] = cameraStatus(camPollIdx);
  camPollIdx = (camPollIdx + 1) % NUM_CAMERAS;
}

int cachedCameraStatus(int cameraNumber) {
  if (cameraNumber < 0 || cameraNumber >= NUM_CAMERAS) return CAMERA_NA;
  return camStatusCache[cameraNumber];
}

// move to visca code 
// Divide into two functions -- isDeadZone() and viscaMapOffset()
int mapOffset(long value, long leftMin, long mid, long leftMax, long rightMin, long rightMax) {
  int th = ( leftMax - leftMin ) * THRESHOLD;
  int midEdgeMax = mid + th;
  int midEdgeMin = mid - th;

  if ( value <= midEdgeMax && value >= midEdgeMin ) return 0;
  if ( value > midEdgeMax ) return map(value, midEdgeMax, leftMax, 1, rightMax);
  if ( value < midEdgeMin ) return map(value, leftMin, midEdgeMin, 1, rightMax + 1) - (rightMax+1);

  return -100;
}

// Recall-ladder thresholds (12-bit ADC on GPIO34), for the simplified one-resistor
// wiring: recall1 is a bare button (GPIO34 -> GND, no resistor), recall2 has a single
// 10k leg to GND, and the onboard 10k pull-up (R48) holds the pin high at rest:
//   none    > 3000        pull-up only (~4095)
//   recall2  1200..3000   (~2048, single 10k leg)
//   recall1  <= 1200      (~0, dead short; the onboard BUT1 ~88 lands here too)
// recall1 is the hard pull to ground, so it always wins: pressing BOTH at once reads
// ~0 and fires recall1 (there is no both-press dead zone in this scheme). The onboard
// BUT1 is a spare recall1. The recall1/recall2 split (1200) sits midway between ~0 and
// ~2048 -- lots of margin; retune from the "recall ladder ADC" log if your leg differs.
#define RECALL_LADDER_NONE_MIN 3000
#define RECALL_LADDER_R2_MIN   1200

// Read the two recall buttons. On the Olimex board they share GPIO34: recall1 is a bare
// button to GND, recall2 has a single 10k leg to GND, and the onboard R48 10k pull-up
// holds the pin high at rest. In the Wokwi sim they are discrete digital pins. r1/r2 are
// true when pressed.
static void readRecallButtons(bool &r1, bool &r2) {
  if (HWRev == REV_OLIMEX) {
    int v = analogRead(PIN_RECALL_2);   // GPIO34 ladder node
    static int lastLogged = -1000;
    if (v < RECALL_LADDER_NONE_MIN && abs(v - lastLogged) > 60) {
      lastLogged = v;
      logi("recall ladder ADC=%d", v);   // calibration aid during bring-up
    }
    r1 = (v <= RECALL_LADDER_R2_MIN);                                 // ~0 dead short (onboard BUT1 lands here too)
    r2 = (v > RECALL_LADDER_R2_MIN && v <= RECALL_LADDER_NONE_MIN);   // ~2048 single 10k leg
  } else {
    r1 = (digitalRead(PIN_RECALL_1) == LOW);
    r2 = (digitalRead(PIN_RECALL_2) == LOW);
  }
}

void buttonLoop() {
  bool r1, r2;
  readRecallButtons(r1, r2);
  if ( r1 && r2 ) {
    // autoCalibrate();  // both pressed -- reserved (can't occur on the analog ladder)
  } else if ( r1 && (wasButton1Pressed == false) ) {
    wasButton1Pressed = true;
    button1Override = overridePreview();    // latch intent at press
    notePresetHeld(1, true);                // pending pill while held (recall fires on release)
  } else if ( !r1 && (wasButton1Pressed == true)){
    wasButton1Pressed = false;
    notePresetHeld(1, false);               // clear pending; confirmation pill follows
    if ( button1Override ) {                // was holding OVERRIDE = save this position
      logi("OVERRIDE + button 1: saving preset 1");
      if (isVisca(getActiveCamera())) visca_set_memory(1);
      else if (settings.cameraType[getActiveCamera()] == CAM_ONVIF) Onvif_SetPreset(1);
      notePreset(1, true);
    } else {                                // tap = recall
      logi("Button 1: recalling preset 1");
      if (isVisca(getActiveCamera())) visca_recall_memory(1);
      else if (settings.cameraType[getActiveCamera()] == CAM_ONVIF) Onvif_GoToPreset(1);
      notePreset(1, false);
    }
  } else if ( r2 && (wasButton2Pressed == false)) {
    wasButton2Pressed = true;
    button2Override = overridePreview();    // latch intent at press
    notePresetHeld(2, true);                // pending pill while held (recall fires on release)
  } else if ( !r2 && (wasButton2Pressed == true)) {
    wasButton2Pressed = false;
    notePresetHeld(2, false);               // clear pending; confirmation pill follows
    if ( button2Override ) {                // was holding OVERRIDE = save this position
      logi("OVERRIDE + button 2: saving preset 2");
      if (isVisca(getActiveCamera())) visca_set_memory(2);
      else if (settings.cameraType[getActiveCamera()] == CAM_ONVIF) Onvif_SetPreset(2);
      notePreset(2, true);
    } else {                                // tap = recall
      logi("Button 2: recalling preset 2");
      if (isVisca(getActiveCamera())) visca_recall_memory(2);
      else if (settings.cameraType[getActiveCamera()] == CAM_ONVIF) Onvif_GoToPreset(2);
      notePreset(2, false);
    }
  } else {
    // TESTING
    // Only send to undo a button press - need to keep state.
    //visca_recall(3);
  }
}

void cameraControlLoop() {
  int pan = analogRead(PIN_PAN);
  int tilt = analogRead(PIN_TILT);
  int zoom = analogRead(PIN_ZOOM);

  // Joystick center recalibration (config page "Recalibrate center"). Done here in
  // loop context because this task owns the ADC. Capture the resting position as
  // the new deadzone center; the trim screws set it at build time, this handles
  // drift. Averaged to shed ADC noise, then persisted.
  if ( g_calibrateCenter ) {
    long p = 0, t = 0, z = 0;
    for ( int i = 0; i < 16; i++ ) { p += analogRead(PIN_PAN); t += analogRead(PIN_TILT); z += analogRead(PIN_ZOOM); delay(3); }
    settings.panMid = p / 16; settings.tiltMid = t / 16; settings.zoomMid = z / 16;
    if ( !InSimulator ) { EEPROM.put(0, settings); EEPROM.commit(); }
    logi("Joystick center calibrated: pan=%u tilt=%u zoom=%u", settings.panMid, settings.tiltMid, settings.zoomMid);
    g_calibrateCenter = false;
  }

  // if inDeadZone( pan ) then pan = 0; same for tilt and zoom

  // if camera changed since previous message send, stop PT and Zoom on previous camera
  //    maybe put this in overridePreviw() handler()

  // check for pan and tilt change -- otherwise skip to zoom check
  // check for pan and tilt stop -- send PT stop - always ASAP
  // otherwise send pantilt command - if SendTimer has elapsed

  // check for zoom change -- otherwise skip to button handlers
  // check for zoom stop -- send zoom stop - always ASAP
  // otherwise send zoom command - if SendTimer has elapsed

  // Deadzone/speed mapping is centered on the calibrated rest position
  // (settings.*Mid, defaults to AnalogMax/2), not a hardcoded midpoint -- so a
  // stick that rests off-center doesn't read as a constant drive.
  int panSpeed = mapOffset(pan, 0, settings.panMid, AnalogMax, -PAN_SPEED_MAX, PAN_SPEED_MAX);
  int tiltSpeed = -1*mapOffset(tilt, 0, settings.tiltMid, AnalogMax, -TILT_SPEED_MAX, TILT_SPEED_MAX);
  int zoomSpeed = mapOffset(zoom, 0, settings.zoomMid, AnalogMax, -ZOOM_SPEED_MAX, ZOOM_SPEED_MAX);

  bool idle = ( panSpeed == 0 && tiltSpeed == 0 && zoomSpeed == 0 );

  // Refresh the camera-status cache from this (main-loop) task, so the async web
  // handler never touches a camera socket concurrently with camera driving.
  // cameraStatus() does blocking socket I/O (up to ~800ms for a down VISCA-UDP
  // camera), so only poll while idle -- never mid-drive, where it would stall
  // the joystick.
  if ( idle ) pollCameraStatus();

  // the last part of this if statement inserts a bit of delay if needed before sending the next command
  // only once MAX_SEND ms -- 100ms max UNLESS YOU ARE TRYING TO STOP THE CAMMERA -- then do that ASAP
  unsigned long currentSendTime = millis();
  if ( idle || ( currentSendTime - LastSendTime > MAX_SEND ) ) {
    // ptzDrive sends two VISCA packets (pan/tilt + zoom). visca_send keeps a
    // single "previous packet" for dup-suppression, so while both axes drive it
    // never matches and every packet goes out at ~10Hz -- which is fine: that is
    // normal continuous-drive behavior, and ptzDrive already suppresses redundant
    // STOPs locally via pt_stopped/zoom_stopped. Per-message-type dedup would be a
    // micro-optimization with no practical benefit, so it is intentionally omitted.
    if (isVisca(getActiveCamera())) {
      ptzDrive(panSpeed, tiltSpeed, zoomSpeed);
    } else if (settings.cameraType[getActiveCamera()] == CAM_ONVIF) {
      Onvif_PtzDrive(panSpeed, tiltSpeed, zoomSpeed);
    }
    LastSendTime = currentSendTime;

    char msg[256];
    sprintf(msg, "{ \"pan\": \"%d\", \"viscaPan\": \"%d\", "
      "\"tilt\": \"%d\", \"viscaTilt\": \"%d\", "
      "\"zoom\": \"%d\", \"viscaZoom\": \"%d\" }"
      ,pan, panSpeed, tilt, tiltSpeed, zoom, zoomSpeed);
    broadcastTelemetry(msg);
  }
  buttonLoop();

  JoystickState js = { pan, tilt, zoom, panSpeed, tiltSpeed, zoomSpeed };
  displayLoop(js);
}