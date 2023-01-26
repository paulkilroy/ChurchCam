#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <EEPROM.h>
#include <Update.h>

#include "globals.h"

AsyncWebServer server(80);

const char* firmwareIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";



String processor(const String& var) {
  if (var == "ATEMCameras")
    return F("");
  if (var == "DiscoveredCameras")
    return F("");
  return String();
}

void handleRoot(AsyncWebServerRequest* request) {
  printf("web request for: %s%s\n", request->host().c_str(), request->url().c_str());

  request->send_P(200, "text/html", config_html, processor);
}

//Serve setup web page to client, by sending HTML with the correct variables
void handleRootOld(AsyncWebServerRequest* request) {
  printf("web request for: %s%s\n", request->host().c_str(), request->url().c_str());

  String html = "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\"><title>PTZ setup</title></head><script>function switchIpField(e){console.log(\"switch\");console.log(e);var target=e.srcElement||e.target;var maxLength=parseInt(target.attributes[\"maxlength\"].value,10);var myLength=target.value.length;if(myLength>=maxLength){var next=target.nextElementSibling;if(next!=null){if(next.className.includes(\"IP\")){next.focus();}}}else if(myLength==0){var previous=target.previousElementSibling;if(previous!=null){if(previous.className.includes(\"IP\")){previous.focus();}}}}function ipFieldFocus(e){console.log(\"focus\");console.log(e);var target=e.srcElement||e.target;target.select();}function load(){var containers=document.getElementsByClassName(\"IP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}containers=document.getElementsByClassName(\"tIP\");for(var i=0;i<containers.length;i++){var container=containers[i];container.oninput=switchIpField;container.onfocus=ipFieldFocus;}toggleStaticIPFields();}function toggleStaticIPFields(){var enabled=document.getElementById(\"staticIP\").checked;document.getElementById(\"staticIPHidden\").disabled=enabled;var staticIpFields=document.getElementsByClassName('tIP');for(var i=0;i<staticIpFields.length;i++){staticIpFields[i].disabled=!enabled;}}</script><style>a{color:#0F79E0}</style><body style=\"font-family:Verdana;white-space:nowrap;\"onload=\"load()\"><table cellpadding=\"2\"style=\"width:100%\"><tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h1>&nbsp;PTZ setup</h1><h2>&nbsp;Status:</h2></td></tr><tr><td><br></td><td></td><td style=\"width:100%;\"></td></tr><tr><td>Connection Status:</td><td colspan=\"2\">";
  switch (WiFi.status()) {
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
  //Commented out for users without batteries
  // html += "<tr><td><br></td></tr><tr><td>Battery voltage:</td><td colspan=\"2\">";
  // html += dtostrf(uBatt, 0, 3, buffer);
  // html += " V</td></tr>";
  html += "<tr><td>Static IP:</td><td colspan=\"2\">";
  html += settings.staticIP == true ? "True" : "False";
  html += "</td></tr><tr><td>PTZ IP:</td><td colspan=\"2\">";
  html += localIP().toString();
  html += "</td></tr><tr><td>Subnet mask: </td><td colspan=\"2\">";
  html += subnetMask().toString();
  html += "</td></tr><tr><td>Gateway: </td><td colspan=\"2\">";
  html += gatewayIP().toString();
  html += "</td></tr><tr><td><br></td></tr><tr><td>ATEM switcher status:</td><td colspan=\"2\">";
  if (atemSwitcher.hasInitialized())
    html += "Connected - Initialized";
  else if (atemSwitcher.isRejected())
    html += "Connection rejected - No empty spot";
  else if (atemSwitcher.isConnected())
    html += "Connected - Wating for initialization";
  else if (WiFi.status() == WL_CONNECTED)
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

  html += "</td></tr><tr><td><br></td></tr><tr bgcolor=\"#777777\"style=\"color:#ffffff;font-size:.8em;\"><td colspan=\"3\"><h2>&nbsp;Settings:</h2></td></tr><tr><td><br></td></tr><form action=\"/save\"method=\"post\"><tr><td>PTZ name: </td><td><input type=\"text\"size=\"30\"maxlength=\"30\"name=\"ptzName\"value=\"";
  html += getHostname();
  html += "\"required/></td></tr>";

  html += "<tr><td><br></td></tr><tr><td>Network name(SSID): </td><td><input type =\"text\"size=\"30\"maxlength=\"30\"name=\"ssid\"value=\"";
  html += getSSID();
  html += "\"required/></td></tr><tr><td>Network password: </td><td><input type=\"password\"size=\"30\"maxlength=\"30\"name=\"pwd\"pattern=\"^$|.{8,32}\"value=\"";
  if (WiFi.isConnected())  //As a minimum security meassure, to only send the wifi password if it's currently connected to the given network.
    html += WiFi.psk();
  html += "\"/></td></tr><tr><td><br></td></tr><tr><td>Use static IP: </td><td><input type=\"hidden\"id=\"staticIPHidden\"name=\"staticIP\"value=\"false\"/><input id=\"staticIP\"type=\"checkbox\"name=\"staticIP\"value=\"true\"onchange=\"toggleStaticIPFields()\"";
  if (settings.staticIP)
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
  if (settings.hideJoystickPosition)
    html += "checked";
  html += "/></td></tr>";

  // Cameras
  for (int i = 0; i < NUM_CAMERAS; i++) {
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

  request->send(200, "text/html", html);
  Serial.println("finished sending html");
}

//Save new settings from client in EEPROM and restart the ESP8266 module
void handleSave(AsyncWebServerRequest* request) {
  if (request->method() != HTTP_POST) {
    request->send(405, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;PTZ setup</h1></td></tr></table><br>Request without posting settings not allowed</body></html>");
  } else {
    String ssid;
    String pwd;
    bool change = false;
    for (uint8_t i = 0; i < request->params(); i++) {
      change = true;
      AsyncWebParameter* p = request->getParam(i);
      String var = p->name();
      String val = p->value();
      // Serial.printf("name: %s - %s\n", var, val);

      if (var == "ptzName") {
        val.toCharArray(settings.ptzName, (uint8_t)32);
      } else if (var == "ssid") {
        ssid = String(val);
      } else if (var == "pwd") {
        pwd = String(val);
      } else if (var == "staticIP") {
        settings.staticIP = (val == "true");
      } else if (var == "sIP1") {
        settings.staticIPAddr[0] = val.toInt();
      } else if (var == "sIP2") {
        settings.staticIPAddr[1] = val.toInt();
      } else if (var == "sIP3") {
        settings.staticIPAddr[2] = val.toInt();
      } else if (var == "sIP4") {
        settings.staticIPAddr[3] = val.toInt();
      } else if (var == "mask1") {
        settings.staticSubnetMask[0] = val.toInt();
      } else if (var == "mask2") {
        settings.staticSubnetMask[1] = val.toInt();
      } else if (var == "mask3") {
        settings.staticSubnetMask[2] = val.toInt();
      } else if (var == "mask4") {
        settings.staticSubnetMask[3] = val.toInt();
      } else if (var == "gate1") {
        settings.staticGateway[0] = val.toInt();
      } else if (var == "gate2") {
        settings.staticGateway[1] = val.toInt();
      } else if (var == "gate3") {
        settings.staticGateway[2] = val.toInt();
      } else if (var == "gate4") {
        settings.staticGateway[3] = val.toInt();
      } else if (var == "aIP1") {
        settings.switcherIP[0] = val.toInt();
      } else if (var == "aIP2") {
        settings.switcherIP[1] = val.toInt();
      } else if (var == "aIP3") {
        settings.switcherIP[2] = val.toInt();
      } else if (var == "aIP4") {
        settings.switcherIP[3] = val.toInt();
      }
      for (int i = 0; i < NUM_CAMERAS; i++) {
        if (var == "c" + String(i + 1) + "IP1") {
          settings.cameraIP[i][0] = val.toInt();
        } else if (var == "c" + String(i + 1) + "IP2") {
          settings.cameraIP[i][1] = val.toInt();
        } else if (var == "c" + String(i + 1) + "IP3") {
          settings.cameraIP[i][2] = val.toInt();
        } else if (var == "c" + String(i + 1) + "IP4") {
          settings.cameraIP[i][3] = val.toInt();
        } else if (var == "c" + String(i + 1) + "Protocol") {
          settings.cameraProtocol[i] = val.toInt();
        } else if (var == "c" + String(i + 1) + "Port") {
          settings.cameraPort[i] = val.toInt();
        } else if (var == "c" + String(i + 1) + "Headers") {
          settings.cameraHeaders[i] = val.toInt();
        }
      }
      if (var == "hideJoystickPosition") {
        settings.hideJoystickPosition = (val == "true");
      }
    }

    if (change) {
      EEPROM.put(0, settings);
      EEPROM.commit();

      request->send(200, "text/html", (String) "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ setup</title></head><body><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"font-family:Verdana;color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp;PTZ setup</h1></td></tr></table><br>Settings saved successfully.</body></html>");

      // Delay to let data be saved, and the response to be sent properly to the client
      // ??? request->close();  // Close server to flush and ensure the response gets to the client
      delay(100);

      // Change into STA mode to disable softAP
      WiFi.mode(WIFI_STA);
      delay(100);  // Give it time to switch over to STA mode (this is important on the ESP32 at least)

      if (ssid && pwd) {
        WiFi.persistent(true);  // Needed by ESP8266
        // Pass in 'false' as 5th (connect) argument so we don't waste time trying to connect, just save the new SSID/PSK
        // 3rd argument is channel - '0' is default. 4th argument is BSSID - 'NULL' is default.
        WiFi.begin(ssid.c_str(), pwd.c_str(), 0, NULL, false);
      }

      //Delay to apply settings before restart
      //delay(100);
      //ESP.restart();
      request->onDisconnect([]() {
        ESP.restart();
      });
    }
  }
}

//Send 404 to client in case of invalid webpage being requested.
void handleNotFound(AsyncWebServerRequest* request) {
  request->send(404, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ Setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp PTZ Setup</h1></td></tr></table><br>404 - Page not found</body></html>");
}

const char text_html[] PROGMEM = "text/html";
const char text_css[] PROGMEM = "text/css";
const char text_javascript[] PROGMEM = "text/javascript";

void webSetup() {
  // Initialize and begin HTTP server for handeling the web interface
  server.on("/", HTTP_GET, handleRoot);
  server.on("/old", HTTP_GET, handleRootOld);
  server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    printf("web request for: %s%s\n", request->host().c_str(), request->url().c_str());
    printf("strlen: %d\n", strlen(bootstrap_min_css));
    String x = bootstrap_min_css;
    printf("String.length: %d\n", x.length() );

    request->send_P(200, text_css, bootstrap_min_css);
  });
  server.on("/headers.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    printf("web request for: %s%s\n", request->host().c_str(), request->url().c_str());
    request->send_P(200, text_css, headers_css);
  });
  server.on("/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    printf("web request for: %s%s\n", request->host().c_str(), request->url().c_str());
    request->send_P(200, text_javascript, bootstrap_bundle_min_js);
  });
  server.on("/validate-forms.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    printf("web request for: %s%s\n", request->host().c_str(), request->url().c_str());
    request->send_P(200, text_javascript, validate_forms_js);
  });

  server.on("/save", HTTP_POST | HTTP_GET, handleSave);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/logData", HTTP_GET, handleLogData);
  server.on("/center", HTTP_GET, [](AsyncWebServerRequest* request) {
    calibrateCenter();
    //request->addHeader("Connection", "close");
    request->send(200, "text/html", "<h1>Calibration complete...</h1>");
  });
  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest* request) {
    autoCalibrate();
    //request->addHeader("Connection", "close");
    request->send(200, "text/html", "<h1>Calibration complete...</h1>");
  });
  server.on("/erase", HTTP_GET, [](AsyncWebServerRequest* request) {
    for (int i = 0; i < sizeof(settings); i++) {
      EEPROM.write(i, 255);
    }
    EEPROM.commit();
    delay(500);
    //request->addHeader("Connection", "close");
    request->send(200, "text/html", "<h1>Erased, please restart...</h1>");
  });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest* request) {
    //request->addHeader("Connection", "close");
    request->send(200, "text/plain", "Restarting...");
    ESP.restart();
  });
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest* request) {
    //request->addHeader("Connection", "close");
    request->send(200, "text/html", firmwareIndex);
  });
  /*handling uploading firmware file */
  server.on(
    "/update", HTTP_POST, [](AsyncWebServerRequest* request) {
      //request->addHeader("Connection", "close");
      request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        }
      }
      /* flashing firmware to ESP*/
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
      if (final) {
        Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
      }
    });
  server.onNotFound(handleRoot);
  server.begin();
}