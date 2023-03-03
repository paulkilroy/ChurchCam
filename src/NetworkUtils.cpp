#include <Arduino.h>
// #include <WiFi.h>

#include "globals.h"

// This file abstracts out TCP and UDP for the main visca send method

// UDP libraries in Arduino abstract to an unconnected state, a connected state
// would give better understanding if a UDP camera is up
// https://blog.cloudflare.com/everything-you-ever-wanted-to-know-about-udp-sockets-but-were-afraid-to-ask-part-1/

#define NETWORK_DEBUG 1

#define VISCA_MAX_WAIT        5 // increased from 20 (2 seconds)
#define VISCA_MAX_RESPONSE    1024

#define VISCA_PROTOCOL_TCP    1
#define VISCA_PROTOCOL_UDP    0



WiFiClient tcpCameraSocket[NUM_CAMERAS];
WiFiUDP udp;
boolean udpSetup = false;

// For debugging packets to strings
void printBytes(byte array[], unsigned int len) {

  if (len > 0) {
    logd( stringBytes( array, len ).c_str() );
  }
}

String stringBytes(byte array[], unsigned int len) {
  int b = 0;
  char buffer[1024];

  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[b++] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[b++] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    buffer[b++] = ' ';
  }
  buffer[b++] = '\0';

  return String(buffer);
}

void writeBytes( uint32_t value, byte packet[], int position ) {
  byte b4 = (byte)(value & 0xFFu);         // LSB
  byte b3 = (byte)((value >> 8) & 0xFFu);
  byte b2 = (byte)((value >> 16) & 0xFFu);
  byte b1 = (byte)((value >> 24) & 0xFFu); // MSB

  packet[position++] = b1;
  packet[position++] = b2;
  packet[position++] = b3;
  packet[position++] = b4;
}

int connect( int cameraNumber ) {
  // TCP is a connected protocol, thats the only one we need to worry about here
  if ( settings.cameraProtocol[cameraNumber] == VISCA_PROTOCOL_TCP ) {
    if ( !tcpCameraSocket[cameraNumber].connected() ) {
      logw("TCP reconnecting to camera: %d %s port: %d\n", cameraNumber + 1, settings.cameraIP[cameraNumber].toString().c_str(), settings.cameraPort[cameraNumber]);

      int status = tcpCameraSocket[cameraNumber].connect(settings.cameraIP[cameraNumber], settings.cameraPort[cameraNumber] );
      if ( !status ) {
        loge("TCP connect to: %s port: %d returned zero: %d\n", settings.cameraIP[cameraNumber].toString().c_str(), settings.cameraPort[cameraNumber], status);
        return NETWORK_ERROR;
      }
    } else {
      // clear out the buffers just in case
      tcpCameraSocket[cameraNumber].flush();
    }
  } else {
    if ( !udpSetup ) {
      udp.begin(VISCA_PORT);
      // udpSetup = true; // Added 12/1/2022 not sure why this wasn't here
    } else {
      // clear out the buffers just in case
      udp.flush(); // Added 12/1/2022 not sure why this wasn't here 
                   // But this actually just blows away the read buffer
    }
  }
  return NETWORK_SUCCESS;
}


int send( int cameraNumber, byte packet[], int size ) {
#ifdef NETWORK_DEBUG
  //loge("sending packet to camera: %d at ip address: %s\n", cameraNumber + 1, settings.cameraIP[cameraNumber].toString().c_str());
#endif
  int written = 0;
  if ( settings.cameraProtocol[cameraNumber] == VISCA_PROTOCOL_TCP ) {
    written = tcpCameraSocket[cameraNumber].write(packet, size);
  } else {
    int status = udp.beginPacket(settings.cameraIP[cameraNumber], settings.cameraPort[cameraNumber] );
    if ( !status ) {
      loge("beginPacket returned zero: %d\n", status);
      return NETWORK_ERROR;
    }

    written = udp.write(packet, size);
    if( size != written ) {
      loge("write() size: %d, written %d\n", size, written);
    }

    status = udp.endPacket();
    if ( !status ) {
      loge("endPacket returned zero: %d\n", status);
      return NETWORK_ERROR;
    }
    
  }

#ifdef NETWORK_DEBUG
  logd("Send() Cam[%d] [%s:%s:%d] %s Bytes[%d] [%s] == ", cameraNumber + 1, 
      settings.cameraProtocol[cameraNumber]==VISCA_PROTOCOL_TCP?"TCP":"UDP", 
      settings.cameraIP[cameraNumber].toString().c_str(), 
      settings.cameraPort[cameraNumber], settings.cameraHeaders[cameraNumber] == 0 ? "EX" : "IN", 
      written, stringBytes(packet, size).c_str());
#endif

  if ( written != size ) {
    loge("Didn't write enough bytes: %d vs %d\n", size, written);
    return NETWORK_ERROR;
  }
  return NETWORK_SUCCESS;
}

int recieve( int cameraNumber, byte packet[] ) {
  int len;
  if ( settings.cameraProtocol[cameraNumber] == VISCA_PROTOCOL_TCP ) {
    boolean msgReady = false;
    for ( int i = 0; i < VISCA_MAX_WAIT; i++) {
      int ret = tcpCameraSocket[cameraNumber].available();

      if ( ret < 0 ) {
        loge("Error checking available()\n");
        return NETWORK_ERROR;
      }
      if ( ret > 0 ) {
        msgReady = true;
        break;
      }
      delay(100);
#ifdef NETWORK_DEBUG
      Serial.print(".");
#endif
    }
    if ( !msgReady ) {
      loge("No response - timeout\n");
      return NETWORK_TIMEOUT;
    } else {
      len = tcpCameraSocket[cameraNumber].read(packet, VISCA_MAX_RESPONSE);
    }
  } else { // UDP
    boolean msgReady = false;
    for ( int i = 0; i < VISCA_MAX_WAIT; i++) {
      if ( udp.parsePacket() > 0 ) {
        msgReady = true;
        break;
      }
      delay(100);
#ifdef NETWORK_DEBUG
      Serial.print(".");
#endif
    }

    if ( !msgReady ) {
      loge("No response - timeout\n");
      return NETWORK_TIMEOUT;
    } else {
      // receive incoming UDP packets
      len = udp.read(packet, VISCA_MAX_RESPONSE);
    }
  }

  if ( 0 > len ) {
    loge("read returned: %d \n", len);
    return NETWORK_ERROR;
  }
#ifdef NETWORK_DEBUG
  //loge("Msg Received %d bytes from %s, port %d: \n", len, udp.remoteIP().toString().c_str(), udp.remotePort());
  //printBytes(packet, len);
  logd("Recv() %d [%s] == ", len, stringBytes(packet, len).c_str());
#endif
  return NETWORK_SUCCESS;
}
