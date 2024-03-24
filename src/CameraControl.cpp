#include <Arduino.h> 
#include <EEPROM.h>
#include "globals.h"
#include <time.h>
/*
#include <WebSocketServer.h>
using namespace net;
*/
WebSocketServer webSocketServer { 3000 };

#define THRESHOLD .1

// If you want to slow down the camera movement just change these from the visca standards below
// 18 is the VISCA max -- too fast

#define ANALOG_RESOLUTION 12
int AnalogMax = pow(2, ANALOG_RESOLUTION) - 1;
#define IS_ACTIVE(X) ((X > AnalogMax/2*.85) && (X < AnalogMax/2*1.15))
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

#define LONG_PRESS_TIME 500

void cameraControlSetup() {
  analogReadResolution(ANALOG_RESOLUTION);  // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
  //analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6.

  for ( HWRev = 0; HWRev < REV_MODELS; HWRev++ ) {
    pinMode(Pinouts[HWRev].tilt, INPUT);
    pinMode(Pinouts[HWRev].pan, INPUT);
    pinMode(Pinouts[HWRev].zoom, INPUT);

    int pan = analogRead(Pinouts[HWRev].pan);
    int tilt = analogRead(Pinouts[HWRev].tilt);
    int zoom = analogRead(Pinouts[HWRev].zoom);

    if ( IS_ACTIVE(pan) && IS_ACTIVE(tilt) && IS_ACTIVE(zoom) ) {
      pinMode(Pinouts[HWRev].recall1, INPUT_PULLUP);
      pinMode(Pinouts[HWRev].recall2, INPUT_PULLUP);
      pinMode(Pinouts[HWRev].oride, INPUT_PULLUP);
      logi("Detected Hardware Version: [%d] %s - %d %d %d", HWRev, Pinouts[HWRev].name, pan, tilt, zoom);
      break;
    }
    logi("Skipping HWRev  %s - %d %d %d", Pinouts[HWRev].name, pan, tilt, zoom);
  }
  if ( HWRev == REV_MODELS ) {
    logi("ERROR: HWRev not found, defaulting to first board");
    HWRev = 0;
  }

  setupDefaults();
  viscaSetup();
  displaySetup();

  // Draw the inital screen once
  displayLoop(0, 0, 0, 0, 0, 0);
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
  if ( !atemSwitcher.isConnected() ) {
    // default to cam 1
    if ( !overridePreview() ) {
      return 0;
    } else {
      return 1;
    }
  } else if ( overridePreview() ) {
    //Serial.printf("atem: %d - %d\n", atemSwitcher.getPreviewInputVideoSource(0), atemSwitcher.getProgramInputVideoSource(0));
    return atemSwitcher.getProgramInputVideoSource(0) - 1;
  } else {
    return atemSwitcher.getPreviewInputVideoSource(0) - 1;
  }
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

void buttonLoop() {
  if ( digitalRead(PIN_RECALL_1) == LOW ) {
    drawButton1();
  }
  if ( digitalRead(PIN_RECALL_2) == LOW) {
    drawButton2();
  }

  if ( digitalRead(PIN_RECALL_1) == LOW && digitalRead(PIN_RECALL_2) == LOW ) {
    // autoCalibrate();
  } else if ( (digitalRead(PIN_RECALL_1) == LOW) && (wasButton1Pressed == false) ) {
    wasButton1Pressed = true;
    startPress = millis();
  } else if ( (digitalRead(PIN_RECALL_1) == HIGH) && (wasButton1Pressed == true)){
    wasButton1Pressed = false;
    endPress = millis();
    if ((endPress - startPress) > LONG_PRESS_TIME) {
      logi("Long press on button 1: setting preset 1");
      if (settings.cameraProtocol[getActiveCamera()] == VISCA_PROTOCOL_TCP) {
        visca_set_memory(1);
      } else if (settings.cameraProtocol[getActiveCamera()] == ONVIF_PROTOCOL_TCP) {
        Onvif_SetPreset(1);
      }
    } else {
      logi("Short press on button 1: going to preset 1");
      if (settings.cameraProtocol[getActiveCamera()] == VISCA_PROTOCOL_TCP) {
        visca_recall_memory(1);
      } else if (settings.cameraProtocol[getActiveCamera()] == ONVIF_PROTOCOL_TCP) {
        Onvif_GoToPreset(1);
      }
    }
  } else if ( (digitalRead(PIN_RECALL_2) == LOW) && (wasButton2Pressed == false)) {
    wasButton2Pressed = true;
    startPress = millis();
  } else if ( (digitalRead(PIN_RECALL_2) == HIGH) && (wasButton2Pressed == true)) {
    wasButton2Pressed = false;
    endPress = millis();
    if ((endPress - startPress) > LONG_PRESS_TIME) {
      logi("Long press on button 2: setting preset 2");
      if (settings.cameraProtocol[getActiveCamera()] == VISCA_PROTOCOL_TCP) {
        visca_set_memory(2);
      } else if (settings.cameraProtocol[getActiveCamera()] == ONVIF_PROTOCOL_TCP) {
        Onvif_SetPreset(2);
      }
    } else {
      logi("Short press on button 2: going to preset 2");
      if (settings.cameraProtocol[getActiveCamera()] == VISCA_PROTOCOL_TCP) {
        visca_recall_memory(2);
      } else if (settings.cameraProtocol[getActiveCamera()] == ONVIF_PROTOCOL_TCP) {
        Onvif_GoToPreset(2);
      }
    }
  } else {
    // TESTING
    // Only send to undo a button press - need to keep state.
    //visca_recall(3);
  }
}

void cameraControlLoop() {
  webSocketServer.listen();

  int pan = analogRead(PIN_PAN);
  int tilt = analogRead(PIN_TILT);
  int zoom = analogRead(PIN_ZOOM);

  // if inDeadZone( pan ) then pan = 0; same for tilt and zoom

  // if camera changed since previous message send, stop PT and Zoom on previous camera
  //    maybe put this in overridePreviw() handler()

  // check for pan and tilt change -- otherwise skip to zoom check
  // check for pan and tilt stop -- send PT stop - always ASAP
  // otherwise send pantilt command - if SendTimer has elapsed

  // check for zoom change -- otherwise skip to button handlers
  // check for zoom stop -- send zoom stop - always ASAP
  // otherwise send zoom command - if SendTimer has elapsed

  // move this translate code to visca and in ONVIF just divide by AnalogMax
  int panSpeed = mapOffset(pan, 0, AnalogMax/2, AnalogMax, -PAN_SPEED_MAX, PAN_SPEED_MAX);
  int tiltSpeed = -1*mapOffset(tilt, 0, AnalogMax/2, AnalogMax, -TILT_SPEED_MAX, TILT_SPEED_MAX);
  int zoomSpeed = mapOffset(zoom, 0, AnalogMax/2, AnalogMax, -ZOOM_SPEED_MAX, ZOOM_SPEED_MAX);

  // the last part of this if statement inserts a bit of delay if needed before sending the next command
  // only once MAX_SEND ms -- 100ms max UNLESS YOU ARE TRYING TO STOP THE CAMMERA -- then do that ASAP
  unsigned long currentSendTime = millis();
  if ( ( panSpeed == 0 && tiltSpeed == 0 && zoomSpeed == 0 ) || ( currentSendTime > LastSendTime + MAX_SEND ) ) {
    // TODO Separate out panTilt and zoom, then put repeate check by message type in 
    // the visca send function
    if (settings.cameraProtocol[getActiveCamera()] == VISCA_PROTOCOL_TCP) {
      ptzDrive(panSpeed, tiltSpeed, zoomSpeed);
    } else if (settings.cameraProtocol[getActiveCamera()] == ONVIF_PROTOCOL_TCP) {
      Onvif_PtzDrive(panSpeed, tiltSpeed, zoomSpeed);
    }
    LastSendTime = currentSendTime;
    char msg[256];
    sprintf(msg, "{ \"pan\": \"%d\", \"viscaPan\": \"%d\", "
      "\"tilt\": \"%d\", \"viscaTilt\": \"%d\", "
      "\"zoom\": \"%d\", \"viscaZoom\": \"%d\" }"
      ,pan, panSpeed, tilt, tiltSpeed, zoom, zoomSpeed);
    webSocketServer.broadcast(WebSocket::DataType::TEXT, msg, strlen(msg));
  }
    buttonLoop();
    displayLoop(pan, tilt, zoom, panSpeed, tiltSpeed, zoomSpeed);
}