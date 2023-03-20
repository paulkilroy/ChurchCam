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

// Serve setup web page to client, by sending HTML with the correct variables
void handleRootOld() {
  logi("web request for: /old");

  String html = "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\"><title>PTZ setup</title></head><script>function switchIpField(e){console.log(\"switch\");console.log(e);var target=e.srcElement||e.target;var maxLength=parseInt(target.attributes[\"maxlength\"].value,10);var myLength=target.value.length;if(myLength>=maxLength){var next=target.nextElementSibling;if(next!=null){if(next.className.includes(\"IP\")){next.focus();}}}else if(myLength==0){var previous=target.previousElementSibling;if(previous!=null){if(previous.className.includes(\"IP\")){previous.focus();}}}}function ipFieldFocus(e){console.log(\"focus\");console.log(e);var target=e.srcElement||e.target;target.select();}function load(){var containers=document.getElementsByClassName(\"IP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}containers=document.getElementsByClassName(\"tIP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}toggleStaticIPFields();}function toggleStaticIPFields(){var enabled=document.getElementById(\"staticIP\").checked;document.getElementById(\"staticIPHidden\").disabled=enabled;var staticIpFields=document.getElementsByClassName('tIP');for(var i=0;i<staticIpFields.length;i++){staticIpFields[i].disabled=!enabled;}}</script><style>a{color:#0F79E0}</style><body style=\"font-family:Verdana;white-space:nowrap;\"onload=\"load()\"><table cellpadding=\"2\"style=\"width:100%\"><tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h1>&nbsp;PTZ setup</h1><h2>&nbsp;Status:</h2></td></tr><tr><td><br></td><td></td><td style=\"width:100%;\"></td></tr><tr><td>Connection Status:</td><td colspan=\"2\">";
  switch ( WiFi.status() ) {
  case WL_CONNECTED:
    html += "Connected to network";
    break;
  case WL_NO_SSID_AVAIL:
    html += "Network not found";
    break;
  case WL_CONNECT_FAILED:
    html += "Invalid password";
    break;
  case WL_IDLE_STATUS:
    html += "Changing state...";
    break;
  case WL_DISCONNECTED:
    html += "Station mode disabled";
    break;
  default:
    html += "Uknown";
    break;
  }
  html += "</td></tr><tr><td>ATEM:</td><td colspan=\"2\">";
  html += atemSwitcher.getATEMmodelname();
  html += " ";
  html += atemSwitcher.isConnected() ? "Up" : "Down";
  html += "</td></tr><tr><td>Board:</td><td colspan=\"2\">";
  html += Pinouts[HWRev].name;
  html += "</td></tr><tr><td>Internal State:</td><td colspan=\"2\">";

  html += "</td></tr><tr><td>Network name (SSID):</td><td colspan=\"2\">";
  html += getSSID();
  html += "</td></tr><tr><td><br></td></tr><tr><td>Signal strength:</td><td colspan=\"2\">";
  html += WiFi.RSSI();
  html += " dBm</td></tr>";

  // Commented out for users without batteries
  //  html += "<tr><td><br></td></tr><tr><td>Battery voltage:</td><td colspan=\"2\">";
  //  html += dtostrf(uBatt, 0, 3, buffer);
  //  html += " V</td></tr>";
  html += "<tr><td>Static IP:</td><td colspan=\"2\">";
  html += settings.staticIP == true ? "True" : "False";
  html += "</td></tr><tr><td>PTZ IP:</td><td colspan=\"2\">";
  html += localIP().toString();
  html += "</td></tr><tr><td>Subnet mask: </td><td colspan=\"2\">";
  html += subnetMask().toString();
  html += "</td></tr><tr><td>Gateway: </td><td colspan=\"2\">";
  html += gatewayIP().toString();
  html += "</td></tr><tr><td><br></td></tr><tr><td>ATEM switcher status:</td><td colspan=\"2\">";
  if ( atemSwitcher.hasInitialized() )
    html += "Connected - Initialized";
  else if ( atemSwitcher.isRejected() )
    html += "Connection rejected - No empty spot";
  else if ( atemSwitcher.isConnected() )
    html += "Connected - Wating for initialization";
  else if ( WiFi.status() == WL_CONNECTED )
    html += "Disconnected - No response from switcher";
  else
    html += "Disconnected - Waiting for WiFi";

  html += "</td></tr><tr><td>ATEM switcher IP:</td><td colspan=\"2\">";
  html += (String)settings.switcherIP[0] + '.' + settings.switcherIP[1] + '.' + settings.switcherIP[2] + '.' + settings.switcherIP[3];
  html += "</td></tr><tr><td><br></td></tr><tr><td>Joystick Pan:</td><td colspan=\"2\">";
  html += String(settings.panMin) + " | " + String(settings.panMid) + " | " + String(settings.panMax);
  html += "</td></tr><tr><td>Joystick Tilt:</td><td colspan=\"2\">";
  html += String(settings.tiltMin) + " | " + String(settings.tiltMid) + " | " + String(settings.tiltMax);
  html += "</td></tr><tr><td>Joystick Zoom:</td><td colspan=\"2\">";
  html += String(settings.zoomMin) + " | " + String(settings.zoomMid) + " | " + String(settings.zoomMax);

  html += "</td></tr><tr><td><br></td></tr><tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h2>&nbsp;Settings:</h2></td></tr><tr><td><br></td></tr><form action=\"/saveold\"method=\"post\"><tr><td>PTZ name: </td><td><input type=\"text\"size=\"30\"maxlength=\"30\"name=\"ptzName\"value=\"";
  html += getHostname();
  html += "\"required/></td></tr>";

  html += "<tr><td><br></td></tr><tr><td>Network name(SSID): </td><td><input type =\"text\"size=\"30\"maxlength=\"30\"name=\"ssid\"value=\"";
  html += getSSID();
  html += "\"required/></td></tr><tr><td>Network password: </td><td><input type=\"password\"size=\"30\"maxlength=\"30\"name=\"pwd\"pattern=\"^$|.{8,32}\"value=\"";
  if ( WiFi.isConnected() ) // As a minimum security meassure, to only send the wifi password if it's currently connected to the given network.
    html += WiFi.psk();
  html += "\"/></td></tr><tr><td><br></td></tr><tr><td>Use static IP: </td><td><input type=\"hidden\"id=\"staticIPHidden\"name=\"staticIP\"value=\"false\"/><input id=\"staticIP\"type=\"checkbox\"name=\"staticIP\"value=\"true\"onchange=\"toggleStaticIPFields()\"";
  if ( settings.staticIP )
    html += "checked";

  html += "/></td></tr><tr><td>PTZ IP: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"sIP1\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticIPAddr[0];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"sIP2\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticIPAddr[1];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"sIP3\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticIPAddr[2];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"sIP4\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticIPAddr[3];
  html += "\"required/></td></tr><tr><td>Subnet mask: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask1\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticSubnetMask[0];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask2\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticSubnetMask[1];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask3\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticSubnetMask[2];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"mask4\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticSubnetMask[3];
  html += "\"required/></td></tr><tr><td>Gateway: </td><td><input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate1\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticGateway[0];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate2\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticGateway[1];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate3\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticGateway[2];
  html += "\"required/>. <input class=\"tIP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"gate4\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.staticGateway[3];
  html += "\"required/></td></tr><tr><td><br></td></tr>";
  html += "<tr><td>ATEM switcher IP: </td><td><input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP1\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.switcherIP[0];
  html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP2\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.switcherIP[1];
  html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP3\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.switcherIP[2];
  html += "\"required/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"aIP4\"pattern=\"\\d{0,3}\"value=\"";
  html += settings.switcherIP[3];
  html += "\"required/></tr><tr><td><br></td></tr>";
  html += "</td></tr><tr><td>Hide Joystick Position: </td><td><input type=\"hidden\"id=\"hideJoystickPositionHidden\"name=\"hideJoystickPosition\"value=\"false\"/><input id=\"hideJoystickPosition\"type=\"checkbox\"name=\"hideJoystickPosition\"value=\"true\"";
  if ( settings.hideJoystickPosition )
    html += "checked";
  html += "/></td></tr>";

  // Cameras
  for ( int i = 0; i < NUM_CAMERAS; i++ ) {
    html += "<tr><td>Camera " + String(i + 1) + " IP: </td><td><input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"c" + String(i + 1) + "IP1\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.cameraIP[i][0];
    html += "\"/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"c" + String(i + 1) + "IP2\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.cameraIP[i][1];
    html += "\"/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"c" + String(i + 1) + "IP3\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.cameraIP[i][2];
    html += "\"/>. <input class=\"IP\"type=\"text\"size=\"3\"maxlength=\"3\"name=\"c" + String(i + 1) + "IP4\"pattern=\"\\d{0,3}\"value=\"";
    html += settings.cameraIP[i][3];
    html += "\"/> Protocol <select class=\"protocol\" name=\"c" + String(i + 1) + "Protocol\">";
    html += "<option value=\"0\" " + String(settings.cameraProtocol[i] == 0 ? "selected" : "") + ">UDP</option>";
    html += "<option value=\"1\" " + String(settings.cameraProtocol[i] != 0 ? "selected" : "") + ">TCP</option></select>";
    html += " Port <input class=\"port\"type=\"text\"size=\"5\"maxlength=\"5\"name=\"c" + String(i + 1) + "Port\"pattern=\"\\d{0,5}\"value=\"";
    html += settings.cameraPort[i];
    html += "\"/> VISCA Headers <select class=\"headers\" name=\"c" + String(i + 1) + "Headers\">";
    html += "<option value=\"0\" " + String(settings.cameraHeaders[i] == 0 ? "selected" : "") + ">Exclude</option>";
    html += "<option value=\"1\" " + String(settings.cameraHeaders[i] != 0 ? "selected" : "") + ">Include</option></select>";
    html += "</tr>";
  }

  html += "<tr><td colspan=2><i>&nbsp;The VISCA standard is UDP / 52381 / Include - please refer to your camera's manual</i></td></tr>";
  html += "<tr><td><br></td></tr>";
  html += "<tr><td/><td style=\"float: right;\"><input type=\"submit\"value=\"Save Changes\"/></td></tr></form><tr bgcolor=\"#cccccc\"style=\"font-size: .8em;\"><td colspan=\"3\"><p>&nbsp;&copy; 2020-2021 <a href=\"https://aronhetlam.github.io/\">Aron N. Het Lam</a></p><p>&nbsp;Based on ATEM libraries for Arduino by <a href=\"https://www.skaarhoj.com/\">SKAARHOJ</a></p></td></tr></table></body></html>";

  srvr.send(200, "text/html", html);
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

// Save new settings from client in EEPROM and restart the ESP8266 module
void handleSaveOld() {
  if ( srvr.method() != HTTP_POST ) {
    srvr.send(405, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;PTZ setup</h1></td></tr></table><br>Request without posting settings not allowed</body></html>");
  } else {
    String ssid;
    String pwd;
    bool change = false;
    for ( uint8_t i = 0; i < srvr.args(); i++ ) {
      change = true;
      String var = srvr.argName(i);
      String val = srvr.arg(i);
      // Serial.printf("name: %s - %s\n", var, val);

      if ( var == "ptzName" ) {
        //val.toCharArray(settings.ptzName, (uint8_t)32);
      } else if ( var == "ssid" ) {
        ssid = String(val);
      } else if ( var == "pwd" ) {
        pwd = String(val);
      } else if ( var == "staticIP" ) {
        settings.staticIP = ( val == "true" );
      } else if ( var == "sIP1" ) {
        settings.staticIPAddr[0] = val.toInt();
      } else if ( var == "sIP2" ) {
        settings.staticIPAddr[1] = val.toInt();
      } else if ( var == "sIP3" ) {
        settings.staticIPAddr[2] = val.toInt();
      } else if ( var == "sIP4" ) {
        settings.staticIPAddr[3] = val.toInt();
      } else if ( var == "mask1" ) {
        settings.staticSubnetMask[0] = val.toInt();
      } else if ( var == "mask2" ) {
        settings.staticSubnetMask[1] = val.toInt();
      } else if ( var == "mask3" ) {
        settings.staticSubnetMask[2] = val.toInt();
      } else if ( var == "mask4" ) {
        settings.staticSubnetMask[3] = val.toInt();
      } else if ( var == "gate1" ) {
        settings.staticGateway[0] = val.toInt();
      } else if ( var == "gate2" ) {
        settings.staticGateway[1] = val.toInt();
      } else if ( var == "gate3" ) {
        settings.staticGateway[2] = val.toInt();
      } else if ( var == "gate4" ) {
        settings.staticGateway[3] = val.toInt();
      } else if ( var == "aIP1" ) {
        settings.switcherIP[0] = val.toInt();
      } else if ( var == "aIP2" ) {
        settings.switcherIP[1] = val.toInt();
      } else if ( var == "aIP3" ) {
        settings.switcherIP[2] = val.toInt();
      } else if ( var == "aIP4" ) {
        settings.switcherIP[3] = val.toInt();
      }
      for ( int i = 0; i < NUM_CAMERAS; i++ ) {
        if ( var == "c" + String(i + 1) + "IP1" ) {
          settings.cameraIP[i][0] = val.toInt();
        } else if ( var == "c" + String(i + 1) + "IP2" ) {
          settings.cameraIP[i][1] = val.toInt();
        } else if ( var == "c" + String(i + 1) + "IP3" ) {
          settings.cameraIP[i][2] = val.toInt();
        } else if ( var == "c" + String(i + 1) + "IP4" ) {
          settings.cameraIP[i][3] = val.toInt();
        } else if ( var == "c" + String(i + 1) + "Protocol" ) {
          settings.cameraProtocol[i] = val.toInt();
        } else if ( var == "c" + String(i + 1) + "Port" ) {
          settings.cameraPort[i] = val.toInt();
        } else if ( var == "c" + String(i + 1) + "Headers" ) {
          settings.cameraHeaders[i] = val.toInt();
        }
      }
      if ( var == "hideJoystickPosition" ) {
        settings.hideJoystickPosition = ( val == "true" );
      }
    }

    if ( change ) {
      if ( !InSimulator ) {
        EEPROM.put(0, settings);
        EEPROM.commit();
      }

      Serial.println("ONSAVE: sending response");
      srvr.send(200, "text/html", (String)"<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ setup</title></head><body><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"font-family:Verdana;color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;PTZ setup</h1></td></tr></table><br>Settings saved successfully.</body></html>");

      // Delay to let data be saved, and the response to be sent properly to the client
      // ??? request->close();  // Close server to flush and ensure the response gets to the client
      // PSK I don't think this is needed anymore with the disconnect handler 
      delay(100);

      // Change into STA mode to disable softAP
      Serial.println("ONSAVE: Going into WiFi");

      WiFi.mode(WIFI_STA);
      delay(100); // Give it time to switch over to STA mode (this is important on the ESP32 at least)

      if ( ssid && pwd ) {
        WiFi.persistent(true); // Needed by ESP8266
        // Pass in 'false' as 5th (connect) argument so we don't waste time trying to connect, just save the new SSID/PSK
        // 3rd argument is channel - '0' is default. 4th argument is BSSID - 'NULL' is default.
        WiFi.begin(ssid.c_str(), pwd.c_str(), 0, NULL, false);
        delay(100);
        Serial.println("Async WiFi.begin() so its stored");
      }

      // Delay to apply settings before restart
      // delay(100);
      Serial.println("ONSAVE: restarting");
      ESP.restart();
      //request->onDisconnect([]() { Serial.println("Saved - Restarting!"); ESP.restart(); });
    }
  }
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
  // Retire these
  srvr.on("/old", handleRootOld);
  srvr.on("/saveold", handleSaveOld);

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

 // TODO Rethink these -- should be able to do this all in software automatically
 // create a UI for the joystick visually with deadzone
   /*
 server.on("/center", HTTP_GET, [](AsyncWebServerRequest* request) {
   calibrateCenter();
   request->send(200, "text/html", "<h1>Calibration complete...</h1>"); });
 server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest* request) {
   autoCalibrate();
   request->send(200, "text/html", "<h1>Calibration complete...</h1>"); });
 */
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
  if ( networkUp() )
    srvr.handleClient();

  if ( Restart ) {
    // gracefully shutdown
    Serial.println("LOOP - shutting down");
    ESP.restart();
  }
}