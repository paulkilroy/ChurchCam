/*#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
*/
#include <WebServer.h>

#include <EEPROM.h>
#include <Update.h>
#include <ESPmDNS.h>

#include "globals.h"

WebServer srvr(80);
String RestartMsg = "";
bool Restart = false;

String processor(const String& var) {
  String serverVars = "";

  //Serial.printf("Processor replace: %s\n", var.c_str());
  if ( var == "Networks" ) {
    //      { "name": "Ethernet", "status": "off", "ip": "10.0.4.40", "info": "tbd" },
    const char nfmt[] = "{ \"name\": \"%s\", \"status\": \"%s\", \"ip\": \"%s\", \"ssid\": \"%s\", \"info\": \"%s\" },\n";
    char network[150];

    String status = "";
    status = WiFi.getStatusBits() & ETH_STARTED_BIT ? "down" : "off";
    if ( WiFi.getStatusBits() & ETH_HAS_IP_BIT ) status = "up";
    sprintf(network, nfmt, "Ethernet", status.c_str(),
      status == "up" ? ETH.localIP().toString().c_str() : "-",
      "-",
      "");
    serverVars += network;

    status = WiFi.getStatusBits() & STA_STARTED_BIT ? "down" : "off";
    if ( WiFi.getStatusBits() & STA_HAS_IP_BIT ) status = "up";
    sprintf(network, nfmt, "WiFi", status.c_str(),
      status == "up" ? WiFi.localIP().toString().c_str() : "-",
      getSSID().c_str(),
      ( String(WiFi.RSSI()) + " dBm" ).c_str());
    serverVars += network;
    //Serial.printf("WiFi.getTxPower() %d\n", WiFi.getTxPower());

    status = WiFi.getStatusBits() & AP_STARTED_BIT ? "up" : "off";
    sprintf(network, nfmt, "Hotspot", status.c_str(),
      status == "up" ? WiFi.softAPIP().toString().c_str() : "-",
      AP_SSID,
      "");
    serverVars += network;
    return serverVars;
  } else if ( var == "ATEMSwitcher" ) {
    //       { "status": "down", "name": "ATEM Mini Pro", "ip": "10.0.4.40", "port": "-", "protocol": "1" },
    const char afmt[] = "{ \"status\": \"%s\", \"name\": \"%s\", \"ip\": \"%s\", \"port\": \"%s\", \"protocol\": \"%s\" }\n";
    char input[150];
    String name = atemSwitcher.getATEMmodelname();
    if ( name == "" ) name = "ATEM Switcher";
    sprintf(input, afmt, atemSwitcher.isConnected() ? "up" : "down",
      name.c_str(),
      settings.switcherIP.toString().c_str(),
      "-",
      "1");
    serverVars += input;
    return serverVars;
  } else if ( var == "ATEMCameras" ) {
    //       { "id": "0", "status": "down", "name": "Camera 1", "ip": "10.0.4.40", "port": "5678", "protocol": "1", "headers": "0" },
    const char cfmt[] = "{ \"id\": \"%d\", \"status\": \"%s\", \"name\": \"%s\", \"ip\": \"%s\", \"port\": \"%d\", \"protocol\": \"%d\", \"headers\": \"%d\" },\n";
    int cameraNumber = 0;
    for ( uint16_t i = 0; i < NUM_CAMERAS; i++ ) {
      // If no switcher than show all potential inputs...
      if ( atemSwitcher.isConnected() && !atemSwitcher.isInputInitialized(i) )
        continue;
      // 0 is an external port input on the ATEM - thats what we want
      if ( atemSwitcher.isConnected() && atemSwitcher.getInputPortType(i) != 0 )
        continue;

      int s = cameraStatus(cameraNumber);
      String status = "na";
      if ( s == CAMERA_UP ) status = "up";
      if ( s == CAMERA_DOWN ) status = "down";
      if ( s == CAMERA_OFF ) status = "off";

      String camName = atemSwitcher.getInputShortName(i);
      if ( camName == "" ) camName = "Camera " + String(i);
      char input[150];
      // Options for camera status 
      //    -compute status from responses(?) or just last response
      //  * -explicit status on page generation (way it was working)
      //    -status in background via ajax (regular polling of networks/atem/cameras)
      //    -status via "update status" ajax button on webpage
      //    Either way don't for get to backout status for non cameras (H2R/AV PC)
      //    i.e. cameras with no IP addresses
      sprintf(input, cfmt, cameraNumber, status.c_str(),
        camName.c_str(),
        settings.cameraIP[cameraNumber].toString().c_str(),
        settings.cameraPort[cameraNumber],
        settings.cameraProtocol[cameraNumber],
        settings.cameraHeaders[cameraNumber]);
      cameraNumber++;
      serverVars += input;
    }
    return serverVars;
  } else if ( var == "STATIC_IP" ) return settings.staticIP ? "checked" : "";
  else if ( var == "STATIC_IP_ADDR" ) return settings.staticIPAddr.toString();
  else if ( var == "STATIC_SUBNET_MASK" ) return settings.staticSubnetMask.toString();
  else if ( var == "STATIC_GATEWAY" ) return settings.staticGateway.toString();
  else if ( var == "ATEM_IP_ADDR" ) return settings.switcherIP.toString();
  else if ( var == "SSID" ) return getSSID();
  else if ( var == "PSK" ) return getPSK();
  else if ( var == "BOARD_NAME" ) return Pinouts[HWRev].name;
  else if ( var == "RESTART_MSG" ) return RestartMsg;

  logi("Unknown replace: %s\n", var.c_str());
  return String();
}

void handleProcessor(const char html_file[] ) {
  logi("web request for: %s\n", srvr.uri().c_str());
  String html = "";

  int size = strlen_P(html_file);
  boolean inVar = false;
  String varName = "";
  int start = 0, end = 0;
  for ( ; end < size; end++ ) {
    char c = pgm_read_word_near(html_file + end);
    if ( inVar ) {
      if ( c == '%' ) {
        inVar = false;
        if ( varName == "" ) {
          start = end;
        } else {
          // Serial.printf("handleRoot: sending var %s\n", varName.c_str());
          String content = processor(varName);
          if ( content.length() > 0 ) html += content;
          varName = "";
          start = end + 1;
        }
      } else {
        varName += c;
      }
    } else if ( c == '%' ) {
      // reached a stopping point, send buffer from 'start' up to 'end'
      inVar = true;
      // Serial.printf("handleRoot: sending %d to %d\n", start, end);
      html.concat(&html_file[start], end - start);
      start = end;
    } else {

    }
  }
  if ( start < end ) {
    html.concat(&html_file[start], end - start);
  }
  srvr.send(200, "text/html", html);
}

void handleRoot() {
  handleProcessor( config_html );
}

void handleRestartAndWait() {
  srvr.sendHeader("Connection", "close");

  if ( RestartMsg == "" ) RestartMsg = "Restart Successful.";

  Serial.println("RESTART - Sending HTML");
  handleProcessor( restart_html );
  Serial.println("RESTART - restart()");

  Restart = true;
}

void handleDiscoverCameras() {
  logi("web request for: %s\n", srvr.uri().c_str());
  //       { "name": "HD Camera", "ip": "10.0.4.40", "port": "5678", "protocol": "0", "headers": "1" },
  String inputs = "["; //"DiscoveredCameras = [";
  const char fmt[] = "{ \"id\": \"%d\", \"name\": \"%s\", \"ip\": \"%s\", \"port\": \"%u\", \"protocol\": \"%d\", \"headers\": \"%d\" }\n";
  int n = discoverCameras();
  if ( n == 0 ) {
    logi("No camera services found...");
  } else {
    logi("%d Camera service(s) found", n);
    for ( int i = 0; i < n; ++i ) {
      if ( i > 0 ) inputs += ',';
      char input[150];
      sprintf(input, fmt, i,
        discoveredCameraName(i).c_str(),
        discoveredCameraIP(i).toString().c_str(),
        VISCA_PORT,
        PROTOCOL_UDP,
        1);
      inputs += input;
    }
  }

  inputs += "]";
  logi("DiscoveredCamers = %s", inputs.c_str());
  srvr.send(200, "application/json", inputs);
}

int getCamNum(String s, String x) {
  String n = s;
  n.replace(x, "");
  return n.toInt();
}
// Save new settings from client in EEPROM and restart the ESP8266 module
void handleSave() {
  logi("SAVE request for: %s\n", srvr.uri().c_str());

  for ( uint8_t i = 0; i < srvr.args(); i++ ) {
    String var = srvr.argName(i);
    String val = srvr.arg(i);
    // Serial.printf("name: %s - %s\n", var.c_str(), val.c_str());

    if ( var == "networkName" ) {
      strcpy(settings.ssid, val.c_str());
    } else if ( var == "networkPassword" ) {
      strcpy(settings.psk, val.c_str());
    } else if ( var == "staticIP" ) {
      settings.staticIP = ( val == "true" );
    } else if ( var == "staticIPAddr" ) {
      settings.staticIPAddr.fromString(val);
    } else if ( var == "staticSubnetMask" ) {
      settings.staticSubnetMask.fromString(val);
    } else if ( var == "staticGateway" ) {
      settings.staticGateway.fromString(val);
    } else if ( var == "atemConfigIP" ) {
      settings.switcherIP.fromString(val);
    } else if ( var.startsWith("camConfigIP") ) {
      int camNum = getCamNum(var, "camConfigIP");
      settings.cameraIP[camNum].fromString(val);
    } else if ( var.startsWith("camConfigProtocol") ) {
      int camNum = getCamNum(var, "camConfigProtocol");
      settings.cameraProtocol[camNum] = val.toInt();
      /*
      <option value="0">VISCA UDP</option>
\      <option value="1">VISCA TCP</option>
      */
      if ( val.toInt() == 1 ) {
        settings.cameraHeaders[camNum] = 0;
      } else {
        settings.cameraHeaders[camNum] = 1;
      }
    } else if ( var.startsWith("camConfigPort") ) {
      int camNum = getCamNum(var, "camConfigPort");
      settings.cameraPort[camNum] = val.toInt();
    } else {
      logi("handleSave(): Unknown var: %s - %s", var.c_str(), val.c_str());
    }
  }

  if ( !InSimulator ) {
    EEPROM.put(0, settings);
    EEPROM.commit();
  }

  RestartMsg = "Successfully updated settings.";
  handleRestartAndWait();
}

// Send 404 to client in case of invalid webpage being requested.
void handleNotFound() {
  logi("web request for: %s\n", srvr.uri().c_str());

  srvr.send(404, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ Setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp PTZ Setup</h1></td></tr></table><br>404 - Page not found</body></html>");
}

const char text_html[] PROGMEM = "text/html";
const char text_css[] PROGMEM = "text/css";
const char text_javascript[] PROGMEM = "text/javascript";

void webSetup() {
  // Initialize and begin HTTP server for handeling the web interface
  srvr.on("/", handleRoot);

  // Send these out as binary gziped files and let the browser cache these
  srvr.on("/bootstrap.min.css", HTTP_GET, []() {
    logi("web request for: %s", srvr.uri().c_str());
    srvr.sendHeader("Cache-Control", "public, max-age=2678400");
    srvr.sendHeader("Content-Encoding", "gzip");
    srvr.send_P(200, text_css, bootstrap_min_css, bootstrap_min_css_bytes); });
  srvr.on("/headers.css", []() {
    logi("web request for: %s", srvr.uri().c_str());
    srvr.sendHeader("Cache-Control", "public, max-age=2678400");
    srvr.sendHeader("Content-Encoding", "gzip");
    srvr.send_P(200, text_css, headers_css, headers_css_bytes); });
  srvr.on("/bootstrap.bundle.min.js", HTTP_GET, []() {
    logi("web request for: %s", srvr.uri().c_str());
    srvr.sendHeader("Cache-Control", "public, max-age=2678400");
    srvr.sendHeader("Content-Encoding", "gzip");
    srvr.send_P(200, text_javascript, bootstrap_bundle_min_js, bootstrap_bundle_min_js_bytes); });
  srvr.on("/validate-forms.js", HTTP_GET, []() {
    logi("web request for: %s", srvr.uri().c_str());
    srvr.sendHeader("Cache-Control", "public, max-age=2678400");
    srvr.sendHeader("Content-Encoding", "gzip");
    srvr.send_P(200, text_javascript, validate_forms_js, validate_forms_js_bytes); });

  srvr.on("/ping", HTTP_GET, []() {
    logi("web request for: %s", srvr.uri().c_str());
    srvr.send(200, "text/plain", "pong"); });
  srvr.on("/discoverCameras", handleDiscoverCameras);
  srvr.on("/save", handleSave);
  srvr.on("/restart", handleRestartAndWait);
  srvr.on("/logData", handleLogData);

  srvr.on("/erase", HTTP_GET, []() {
    for ( int i = 0; i < sizeof(settings); i++ ) {
      EEPROM.write(i, 255);
    }
    EEPROM.commit();
    RestartMsg = "All Data Erased.";
    handleRestartAndWait(); });

  // handling uploading firmware file
  srvr.on("/update", HTTP_POST, []() {
    RestartMsg = "Firmware Update: ";
    RestartMsg += Update.hasError() ? "FAIL" : "OK";
    RestartMsg += ".";
    srvr.send(200, "text/plain", "pong");
    // When this returns, the javascript calls /restart which performs the reboot
    // handleRestartAndWait();
    },
    []() {
      HTTPUpload& upload = srvr.upload();
      if ( upload.status == UPLOAD_FILE_START ) {
        Serial.printf("UPDATE: %s\n", upload.filename.c_str());
        if ( !Update.begin(UPDATE_SIZE_UNKNOWN) ) { //start with max available size
          Update.printError(Serial);
        }
      } else if ( upload.status == UPLOAD_FILE_WRITE ) {
        // Serial.print(".");
        /* flashing firmware to ESP*/
        if ( Update.write(upload.buf, upload.currentSize) != upload.currentSize ) {
          Update.printError(Serial);
        }
      } else if ( upload.status == UPLOAD_FILE_END ) {
        if ( Update.end(true) ) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });

  srvr.onNotFound(handleNotFound);

  srvr.begin();
}

void webLoop() {
  // Handle web interface
  //logi("Checking if network is up");
  if ( networkUp() ) {
    //logi("Before handleClient");
    srvr.handleClient();
    //logi("Network up, handling client");
  }

  if ( Restart ) {
    // gracefully shutdown
    Serial.println("LOOP - shutting down");
    ESP.restart();
  }
}