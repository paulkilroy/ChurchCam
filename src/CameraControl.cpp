#include <Arduino.h> 
#include <EEPROM.h>
#include "globals.h"
/*
#include <WebSocketServer.h>
using namespace net;
*/
WebSocketServer webSocketServer { 3000 };

#define THRESHOLD .1

// If you want to slow down the camera movement just change these from the visca standards below
// 18 is the VISCA max -- too fast
#define PAN_SPEED_MAX 15
#define TILT_SPEED_MAX 15
#define ZOOM_SPEED_MAX 7

#define ANALOG_RESOLUTION 12
int AnalogMax = pow(2, ANALOG_RESOLUTION) - 1;
#define IS_ACTIVE(X) ((X > AnalogMax/2*.85) && (X < AnalogMax/2*1.15))

// int NoJoystick = 1;  // Just in case
unsigned long LastSend = 0;
#define MAX_SEND 100  // Do not send a PTZ message more than every 100ms, unless its a stop

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
    settings.panMid = AnalogMax/2;//950;
    settings.panMax = AnalogMax;
    settings.tiltMin = 0;
    settings.tiltMid = AnalogMax/2;//950;
    settings.tiltMax = AnalogMax;
    settings.zoomMin = 0;
    settings.zoomMid = AnalogMax/2;//950;
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

void calibrateCenter() {
  String instruction = "Center Joystick";
  displayCalibrateScreen(instruction, 0, 0, 0, 0);

  // Run for a whole minute
  for ( int i = 0; i < 2000; i++ ) {
    int p = analogRead(PIN_PAN);
    int t = analogRead(PIN_TILT);
    int z = analogRead(PIN_ZOOM);

    Serial.printf("Mid: %d: %d - %d - %d\n", i, p, t, z);
    displayCalibrateScreen(instruction, i / 20, p, t, z);
    delay(30);
  }
}

void autoCalibrate() {
  String instruction = "Center Joystick";
  displayCalibrateScreen(instruction, 0, 0, 0, 0);

  // listen for 3 seconds takings readings ever 10ms and average
  int panInt = 0;
  int tiltInt = 0;
  int zoomInt = 0;
  for ( int i = 0; i < 100; i++ ) {
    int p = analogRead(PIN_PAN);
    int t = analogRead(PIN_TILT);
    int z = analogRead(PIN_ZOOM);
    panInt += p;
    tiltInt += t;
    zoomInt += z;
    Serial.printf("Mid: %d: %d - %d - %d\n", i, p, t, z);
    displayCalibrateScreen(instruction, i, p, t, z);
    delay(30);
  }
  settings.panMid = (uint16_t)( panInt / 100 );
  settings.tiltMid = (uint16_t)( tiltInt / 100 );
  settings.zoomMid = (uint16_t)( zoomInt / 100 );
  Serial.printf("panMid: %d tiltMid: %d zoomMid: %d\n", settings.panMid, settings.tiltMid, settings.zoomMid);

  // Change display line 2 to "Keep joystick in lower right for 3 seconds"
  instruction = "Lower Right";
  // listen for 3 seconds aking readings every 10ms and take the max X and Y (or Min Y?)
  uint16_t pan = 0;
  uint16_t tilt = 0;
  for ( int i = 0; i < 100; i++ ) {
    pan = max(pan, analogRead(PIN_PAN));
    tilt = max(tilt, analogRead(PIN_TILT));
    displayCalibrateScreen(instruction, i, pan, tilt, 0);
    delay(30);
  }
  settings.panMax = pan;
  settings.tiltMax = tilt;
  Serial.printf("panMax: %d tiltMax: %d \n", settings.panMax, settings.tiltMax);

  // Change display line 2 to "Keep joystick in upper left for 3 seconds"
  instruction = "Upper Left";
  pan = UINT16_MAX;
  tilt = UINT16_MAX;
  for ( int i = 0; i < 100; i++ ) {
    pan = min(pan, analogRead(PIN_PAN));
    tilt = min(tilt, analogRead(PIN_TILT));
    displayCalibrateScreen(instruction, i, pan, tilt, 0);
    delay(30);
  }
  settings.panMin = pan;
  settings.tiltMin = tilt;
  Serial.printf("panMin: %d tiltMin: %d \n", settings.panMin, settings.tiltMin);

  instruction = "Zoom In";
  uint16_t zoom = 0;
  for ( int i = 0; i < 100; i++ ) {
    zoom = max(zoom, analogRead(PIN_ZOOM));
    displayCalibrateScreen(instruction, i, 0, 0, zoom);
    delay(30);
  }
  settings.zoomMax = zoom;
  Serial.printf("zoomMax: %d\n", settings.zoomMax);

  instruction = "Zoom Out";
  zoom = UINT16_MAX;
  for ( int i = 0; i < 100; i++ ) {
    zoom = min(zoom, analogRead(PIN_ZOOM));
    displayCalibrateScreen(instruction, i, 0, 0, zoom);
    delay(30);
  }
  settings.zoomMin = zoom;
  Serial.printf("zoomMin: %d\n", settings.zoomMin);

  // Store in settings and save them
  EEPROM.put(0, settings);
  EEPROM.commit();
}

/*
long map2(long x, long in_min, long in_max, long out_min, long out_max) {
  return ( x - in_min ) * ( out_max - out_min ) / ( in_max - in_min ) + out_min;
}

long translate(long value, long leftMin, long leftMax, long rightMin, long rightMax) {
  // Figure out how 'wide' each range is
  long leftSpan = leftMax - leftMin;
  long rightSpan = rightMax - rightMin;

  // Convert the left range into a 0-1 range (float)
  float valueScaled = (float)( value - leftMin ) / (float)( leftSpan );

  // Convert the 0-1 range into a value in the right range.
  return rightMin + ( valueScaled * rightSpan );
}
*/

int mapOffset(long value, long leftMin, long mid, long leftMax, long rightMin, long rightMax) {
  int th = ( leftMax - leftMin ) * THRESHOLD;
  int midEdgeMax = mid + th;
  int midEdgeMin = mid - th;

  if ( value <= midEdgeMax && value >= midEdgeMin ) return 0;
  if ( value > midEdgeMax ) return map(value, midEdgeMax, leftMax, 1, rightMax);
  if ( value < midEdgeMin ) return map(value, leftMin, midEdgeMin, 1, rightMax + 1) - (rightMax+1);

  return -100;
}
/*
int mapOffset(int input, int in_min, int in_mid, int in_max, int out_min, int out_max) {
  int in_threshold = ( in_max - in_min ) * THRESHOLD;  // 10% dead zone on each side

  //Serial.printf("input: %d in_min: %d in_mid: %d in_max: %d in_threshold: %d\n", input, in_min, in_mid, in_max, in_threshold);
  delay(1000);
  if ( input > (in_mid - in_threshold) && input < ( in_mid + in_threshold ) ) {
    //Serial.println("ignoring");
    return 0;
  } else if ( input > in_mid ) {
    return translate(input, in_mid + in_threshold, in_max, 1, out_max);
  } else {
    Serial.printf("input: %d in_min: %d in_mid-in_threshold: %d -1*out_max: %d -1: -1 == %ld\n", input, in_min, in_mid - in_threshold, -1 * out_max, map(input, in_min, in_mid - in_threshold, -1 * out_max, -1));

    int imin = in_min;
    int imax = in_mid - in_threshold;
    int omin = -1 * out_max;
    int omax = -1;
    Serial.printf("input: %d in_min: %d in_max: %d out_min: %d out_max: %d == result: %d\n",
      input,
      imin,
      imax,
      omin,
      omax,
      (int)translate(input, imin, imax, omin, omax));
    return translate(input, in_min, in_mid - in_threshold, -1 * out_max, -1);
  }
}
*/

void buttonLoop() {
  if ( digitalRead(PIN_RECALL_1) == LOW && digitalRead(PIN_RECALL_2) == LOW ) {
    // autoCalibrate();
  } else if ( digitalRead(PIN_RECALL_1) == LOW ) {
    Serial.println("recall 1");
    drawButton1();
    visca_recall(1);
  } else if ( digitalRead(PIN_RECALL_2) == LOW ) {
    Serial.println("recall 2");
    drawButton2();
    visca_recall(2);
  } else {
    // TESTING
    // Only send to undo a button press - need to keep state.
    //visca_recall(3);
  }
}

void cameraControlLoop() {
  buttonLoop();
  webSocketServer.listen();

  int pan = analogRead(PIN_PAN);
  int tilt = analogRead(PIN_TILT);
  int zoom = analogRead(PIN_ZOOM);

  //int panSpeed = mapOffset(pan, settings.panMin, settings.panMid, settings.panMax, -PAN_SPEED_MAX, PAN_SPEED_MAX);
  //int tiltSpeed = -1*mapOffset(tilt, settings.tiltMin, settings.tiltMid, settings.tiltMax, -TILT_SPEED_MAX, TILT_SPEED_MAX);
  //int zoomSpeed = mapOffset(zoom, settings.zoomMin, settings.zoomMid, settings.zoomMax, -ZOOM_SPEED_MAX, ZOOM_SPEED_MAX);
  int panSpeed = mapOffset(pan, 0, AnalogMax/2, AnalogMax, -PAN_SPEED_MAX, PAN_SPEED_MAX);
  int tiltSpeed = -1*mapOffset(tilt, 0, AnalogMax/2, AnalogMax, -TILT_SPEED_MAX, TILT_SPEED_MAX);
  int zoomSpeed = mapOffset(zoom, 0, AnalogMax/2, AnalogMax, -ZOOM_SPEED_MAX, ZOOM_SPEED_MAX);
  if ( panSpeed != 0 || tiltSpeed != 0 || zoomSpeed != 0 ) {
    //printf("%d %d %d :: %d %d %d\n", pan, tilt, zoom, panSpeed, tiltSpeed, zoomSpeed);
  }

  unsigned long curr = millis();
  if ( ( panSpeed == 0 && tiltSpeed == 0 && zoomSpeed == 0 ) || ( curr > LastSend + MAX_SEND ) ) {
    displayLoop(pan, tilt, zoom, panSpeed, tiltSpeed, zoomSpeed);
    // TODO Separate out panTilt and zoom, then put repeate check by message type in 
    // the visca send function
    ptzDrive(panSpeed, tiltSpeed, zoomSpeed);
    LastSend = curr;
    char msg[256];
    sprintf(msg, "{ \"pan\": \"%d\", \"viscaPan\": \"%d\", "
      "\"tilt\": \"%d\", \"viscaTilt\": \"%d\", "
      "\"zoom\": \"%d\", \"viscaZoom\": \"%d\" }"
      ,pan, panSpeed, tilt, tiltSpeed, zoom, zoomSpeed);
    webSocketServer.broadcast(WebSocket::DataType::TEXT, msg, strlen(msg));
  }
}