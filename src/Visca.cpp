#include <Arduino.h>
#include <WiFi.h>

#include "globals.h"

// Another visca controller: https://github.com/misterhay/VISCA-IP-Controller
// https://github.com/International-Anglican-Church/visca-joystick
// wait wait time for a visca packet to come back from the camera

#define VISCA_ERROR(code) \
  logi("%s%s",displayString.c_str(),code); \
  return;

// digitalWrite(PIN_TRANSMIT, LOW);

//#define VISCA_DEBUG 1

byte visca_empty[256] = {};
byte visca_previous[256] = {};
byte visca_response[256] = {};


#define VISCA_HEADER_SIZE 8

#define VISCA_PWR_INQ_BYTE 2
byte visca_pwr_inq_bytes[] = { 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x81, 0x09, 0x04, 0x00, 0xFF };

#define RECALL_NUM_BYTE 13
byte visca_recall_bytes[] = { 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x02, 0x81, 0x01, 0x04, 0x3f, 0x02, 0x02, 0xff };
//                            [payload ]  [ payload]  [   sequence number  ]  [           payload                    ]
//                              type        length        LSB big-endian
// Change bytes 12 and 13 for pan and tilt speed
#define PAN_SPEED_BYTE 12
#define TILT_SPEED_BYTE 13
#define PT_DIR1_BYTE 14
#define PT_DIR2_BYTE 15
byte visca_pt_drive_bytes[] = { 0x01, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x02, 0x81, 0x01, 0x06, 0x01, 0x00, 0x00, 0x03, 0x03, 0xff };
//                                                                                    [    PT Drive   ][PSpd][TSpd][Direction] [TERM]

// 8x 01 04 07 2p FF
#define ZOOM_DIR_BYTE 12
#define ZOOM_TELE 2 << 8
#define ZOOM_WIDE 3 << 8
byte visca_zoom_bytes[] = { 0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x81, 0x01, 0x04, 0x07, 0x00, 0xff };


// 8x 09 04 47 FF
byte visca_zoom_inq_bytes[] = { 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x81, 0x09, 0x04, 0x47, 0xff };
#define ZOOM_INQ_MIN 0x0000
#define ZOOM_INQ_MAX 0x6000
byte visca_zoom_inq_resp_bytes[] = { 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x50, 0x0e, 0x0e, 0x0e, 0x0e, 0xFF };

uint32_t sequenceNumber = 1;

boolean pt_stopped = true;
boolean zoom_stopped = true;

int previousCamera = 0;

// PSK BUG Keep a UDP socket around for each camera so I'm not creating each one every message
// SONY MANUAL:
// If the multiple commands are send without waiting for the reply, the possibility of non-execution of the command and errors due to
// buffer overflow become high, because of limitations of order to receive commands or execution interval of command.
// It may cause efficiency to be reduced substantially.

void visca_send(String command, byte packet[], int size, int cameraNumber, boolean waitForAck = false, boolean waitForComplete = false, byte response[] = visca_response) {
  WiFiUDP udp;
  String returnCode = "OK";
  String displayString = String(sequenceNumber) + String(" - ") + command + String(" - ");
  //byte response[256] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  memset(response, 0, 256);

  // digitalWrite(PIN_TRANSMIT, HIGH);

  // NUM_CAMERA+1 for hidden broadcast camera
  if (cameraNumber < 0 || cameraNumber > NUM_CAMERAS + 1) {
    loge("Invalid cameraNumber: %d\n", cameraNumber);
    VISCA_ERROR("E99");
  }

  if (!networkUp() || settings.cameraIP[cameraNumber][0] == 0) {
    VISCA_ERROR("E98");
  }

  // Don't keep sending the same packet -- compare to previous
  if (previousCamera == cameraNumber && 0 == memcmp(visca_previous, packet, size)) {
#ifdef VISCA_DEBUG
    logd("Duplicate packet on same camera: ignoring");
    logd("previousCamera: %d cameraNumber: %d\n", previousCamera + 1, cameraNumber + 1);
    printBytes(visca_previous, size);
    printBytes(packet, size);
#endif
    // digitalWrite(PIN_TRANSMIT, LOW);
    return;
  }

  // TODO BUG
  // Write the camera number into the socket portion -- ? maybe ? this wouldn't work if headers are trimmed?

  // Write the sequence number into the visca packet
  writeBytes(sequenceNumber, packet, 4);
  sequenceNumber++;

  // Trim off the header if needed
  int startPos = 0;
  int sendSize = size;
  int successByte = 9;

  /**
      Reply Packet Note
      Ack X0 4Y FF Y = socket number
      Completion (Commands) X0 5Y FF Y = socket number
      Completion (Inquiries) X0 5Y ... FF Y = socket number
      X = 9 to F: Address of the unit + 8
    * * Locked to “X = 9” for VISCA over IP
      Error Packet Description
      X0 6Y 01 FF Message length error
      X0 6Y 02 FF Syntax Error
      X0 6Y 03 FF Command buffer full
      X0 6Y 04 FF Command canceled
      X0 6Y 05 FF No socket (to be canceled)
      X0 6Y 41 FF Command not executable
      X = 9 to F: Address of the unit + 8, Y = socket number
  */
  if (!settings.cameraHeaders[cameraNumber]) {
    startPos = VISCA_HEADER_SIZE;
    sendSize -= VISCA_HEADER_SIZE;
    successByte -= VISCA_HEADER_SIZE;
  }

  if (NETWORK_SUCCESS != connect(cameraNumber)) {
    VISCA_ERROR("E97");
  }

  if (NETWORK_SUCCESS != send(cameraNumber, &packet[startPos], sendSize)) {
    VISCA_ERROR("E96");
  }

  if (waitForAck) {
    int len = recieve(cameraNumber, response);
    if (NETWORK_ERROR == len) {
      VISCA_ERROR("E95");
    } else if (NETWORK_TIMEOUT == len) {
      VISCA_ERROR("EATO");  // PSK Should we try to resend here? -- Resend should happen automatically because we didn't copy
                            // the current command into the prev command buffer
    }

    // Check for valid ACK
    if (0x60 == (response[successByte] & 0xF0)) {
      logw("Invalid ACK: ");
      printBytes(response, 20);
      waitForComplete = false;
      String error = "EA" + String(stringBytes(&response[successByte + 1], 1));
      VISCA_ERROR(error.c_str());
    } else if (0x50 == (response[successByte] & 0xF0)) {
      waitForComplete = false;
    }
  }

  if (waitForComplete) {
    int len = recieve(cameraNumber, response);
    if (NETWORK_ERROR == len) {
      VISCA_ERROR("E93");

    } else if (NETWORK_TIMEOUT == len) {
      VISCA_ERROR("ECTO");
    }
    // Check for valid COMPLETE
    if (0x60 == (response[successByte] & 0xF0)) {
      logw("Invalid COMPLETE: ");
      printBytes(response, 20);
      String error = "EC" + String(stringBytes(&response[successByte + 1], 1));
      VISCA_ERROR(error.c_str());
    }
  }

  memcpy(visca_previous, packet, size);
  previousCamera = cameraNumber;

  logi("%s%s", displayString.c_str(), returnCode.c_str());
  // digitalWrite(PIN_TRANSMIT, LOW);
  //Serial.println("[done]");
}

// Maybe Break this into viscaPTDrive() and viscaZoom()
void ptzDrive(int panSpeed, int tiltSpeed, int zoomSpeed) {
  int activeCamera = getActiveCamera();

  // First check to see if we switch the preview camera in the middle of a pan/tilt or zoom
  // If so, stop the movement on the previous camera
  // Serial.printf("ptzLoop: previousCamera: %d, pt_stopped: %d, zoom_stopped: %d\n", previousCamera, pt_stopped, zoom_stopped);
  if (previousCamera != activeCamera) {
    if (!pt_stopped) {
      visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
      visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
      logw("Stopping pt on previous camera\n");
      visca_send("PT-DRIVE", visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), previousCamera);
    }
    if (!zoom_stopped) {
      visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x0;
      logw("Stopping zoom on previous camera\n");
      visca_send("ZOOM", visca_zoom_bytes, sizeof(visca_zoom_bytes), previousCamera);
    }
  }

  // Maybe get ridof these.. totally unnessesary and confusing
  // Just ignore the visca command if it is a duplicate in viscaSend()

  // This would be good to do but PTDrive and Zoom are two separate commands
  // And this function sends 2 packets every time -- one for PanTilt and one for Zoom
  // Might need to remember the last packet of each type so we don't flood

  boolean pt_stopping = false;
  boolean zoom_stopping = false;

  visca_pt_drive_bytes[PAN_SPEED_BYTE] = byte(abs(panSpeed));
  visca_pt_drive_bytes[TILT_SPEED_BYTE] = byte(abs(tiltSpeed));

  if (panSpeed > 0 && tiltSpeed > 0) {  // upright
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x01;
  } else if (panSpeed < 0 && tiltSpeed < 0) {  // downleft
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x02;
  } else if (panSpeed > 0 && tiltSpeed < 0) {  // downright
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x02;
  } else if (panSpeed < 0 && tiltSpeed > 0) {  // upleft
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x01;
  } else if (panSpeed > 0) {  // right
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
  } else if (panSpeed < 0) {  // left
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
  } else if (tiltSpeed > 0) {  // up
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x01;
  } else if (tiltSpeed < 0) {  // down
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x02;
  } else {  // stop
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
    pt_stopping = true;
  }
  if (!(pt_stopped == true && pt_stopping == true)) {
    visca_send("PT-DRIVE", visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), activeCamera);
  }

  if (zoomSpeed > 0) {
    visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x02;  //ZOOM_TELE + byte(abs(zoomSpeed));
  } else if (zoomSpeed < 0) {
    visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x03;  // ZOOM_WIDE + byte(abs(zoomSpeed));
  } else {
    visca_zoom_bytes[ZOOM_DIR_BYTE] = 0x0;  // Stop
    zoom_stopping = true;
  }
  if (!(zoom_stopped == true && zoom_stopping == true)) {
    visca_send("ZOOM", visca_zoom_bytes, sizeof(visca_zoom_bytes), activeCamera);
  }

  pt_stopped = pt_stopping;
  zoom_stopped = zoom_stopping;
}

void visca_recall(int pos) {
  // testing button presses
  /** TESTING
  int activeCamera = getActiveCamera();

  if( pos == 1) {
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x01; // left
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;   
    visca_send("PT-DRIVE", visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), activeCamera);
  } else if( pos == 2) { 
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x02; // right
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
    visca_send("PT-DRIVE", visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), activeCamera);
  } else if( pos == 3 ) {
    visca_pt_drive_bytes[PT_DIR1_BYTE] = 0x03;
    visca_pt_drive_bytes[PT_DIR2_BYTE] = 0x03;
    visca_send("PT-DRIVE", visca_pt_drive_bytes, sizeof(visca_pt_drive_bytes), activeCamera);
  }
*/

  visca_recall_bytes[RECALL_NUM_BYTE] = byte(pos);
  // Serial.printf("in recall, active camera: %d previousCamera: %d\n", getActiveCamera()+1, previousCamera+1);
  visca_send("RECALL", visca_recall_bytes, sizeof(visca_recall_bytes), getActiveCamera());
}

void viscaSetup() {
  if (FirstTimeSetup) {
    // Config 9th camera
    settings.cameraIP[NUM_CAMERAS][0] = 255;
    settings.cameraIP[NUM_CAMERAS][1] = 255;
    settings.cameraIP[NUM_CAMERAS][2] = 255;
    settings.cameraIP[NUM_CAMERAS][3] = 255;
    settings.cameraPort[NUM_CAMERAS] = VISCA_PORT;
    settings.cameraProtocol[NUM_CAMERAS] = PROTOCOL_UDP;
    settings.cameraHeaders[NUM_CAMERAS] = 1;

    for (int i = 0; i < NUM_CAMERAS; i++) {
      settings.cameraPort[i] = 52381;
      settings.cameraHeaders[i] = 1;
    }
  }
}

int cameraStatus(int cameraNumber) {
  byte response[256];

  if (!networkUp() || settings.cameraIP[cameraNumber][0] == 0 || 
    settings.cameraIP[cameraNumber][0] == 255) {
    return CAMERA_NA;
  }

  int pwrByte = VISCA_HEADER_SIZE + VISCA_PWR_INQ_BYTE;
  if (!settings.cameraHeaders[cameraNumber]) {
    pwrByte -= VISCA_HEADER_SIZE;
  }
  visca_send("PWR_INQ", visca_pwr_inq_bytes, sizeof(visca_pwr_inq_bytes), cameraNumber, true, true, response);

  if (response[pwrByte] == 0x02) {
    return CAMERA_UP;
  } else if (response[pwrByte] == 0x03) {
    return CAMERA_OFF; // Do something different? camera is there but on standy
  } else {
    return CAMERA_DOWN; // camera didn't response
  }
}

void discoverCameras(bool camIPs[]) {
  // WiFiUDP.parsePacket() sets WiFiUDP.remoteIP() on each packet recieved
  byte response[256];
  int startPos = 0;
  int sendSize = sizeof(visca_pwr_inq_bytes);
  int successByte = 9;
  //bool camIPs[256];

  // Maybe just use IP with last octlet as 255?
  memset(response, 0, sizeof(response));
  if (!settings.cameraHeaders[CAMERA_BROADCAST]) {
    startPos = VISCA_HEADER_SIZE;
    sendSize -= VISCA_HEADER_SIZE;
    successByte -= VISCA_HEADER_SIZE;
  }

  connect(CAMERA_BROADCAST);
  send(CAMERA_BROADCAST, &visca_pwr_inq_bytes[startPos], sendSize);
  while (recieve(CAMERA_BROADCAST, response)) {
    IPAddress cam = udp.remoteIP();
    //gethostbyaddr() doesn't exist in arduino / esp32
    printf("Discovered camera at IP: %s\n", cam.toString().c_str());
    camIPs[cam[3]] = true;
  }
}
