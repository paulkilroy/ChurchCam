#include <PsychicHttp.h>
#include <TemplatePrinter.h>

#include <WiFi.h>
#include <EEPROM.h>
#include <Update.h>
#include <ESPmDNS.h>

#include "globals.h"

// One async PsychicHttp server on port 80 serves both the HTTP config UI and the
// joystick/PTZ telemetry WebSocket (/ws) -- replacing the old synchronous Arduino
// WebServer plus the separate mWebSockets server on port 3000.
static PsychicHttpServer server;
static PsychicWebSocketHandler ptzWs;

// Web assets embedded into flash via board_build.embed_txtfiles / embed_files
// (platformio.ini). objcopy names the symbols _binary_html_<name>_start/_end,
// which we alias to friendly names here. The HTML templates come from
// embed_txtfiles so they are null-terminated (safe to print as C strings);
// the css/js are pre-gzipped raw binaries, so we serve them by [start,end).
extern const char config_html[]  asm("_binary_html_config_html_start");
extern const char restart_html[] asm("_binary_html_restart_html_start");

extern const uint8_t bootstrap_css_gz[]     asm("_binary_html_bootstrap_min_css_gz_start");
extern const uint8_t bootstrap_css_gz_end[] asm("_binary_html_bootstrap_min_css_gz_end");
extern const uint8_t bootstrap_js_gz[]      asm("_binary_html_bootstrap_bundle_min_js_gz_start");
extern const uint8_t bootstrap_js_gz_end[]  asm("_binary_html_bootstrap_bundle_min_js_gz_end");
extern const uint8_t headers_css_gz[]       asm("_binary_html_headers_css_gz_start");
extern const uint8_t headers_css_gz_end[]   asm("_binary_html_headers_css_gz_end");
extern const uint8_t validate_js_gz[]       asm("_binary_html_validate_forms_js_gz_start");
extern const uint8_t validate_js_gz_end[]   asm("_binary_html_validate_forms_js_gz_end");

// handleLogData lives in Logging.cpp (it owns the log ring buffer).
esp_err_t handleLogData(PsychicRequest* request, PsychicResponse* response);

String RestartMsg = "";
bool Restart = false;
volatile bool g_otaActive = false;      // set while an OTA upload is streaming (display OTA screen)
volatile uint32_t g_otaBytes = 0;
volatile uint32_t g_otaLastChunk = 0;   // millis() of the last chunk, for stuck-upload detection

String processor(const String& var) {
  String serverVars = "";

  //Serial.printf("Processor replace: %s\n", var.c_str());
  if ( var == "Networks" ) {
    //      { "name": "Ethernet", "status": "off", "ip": "10.0.4.40", "info": "tbd" },
    const char nfmt[] = "{ \"name\": \"%s\", \"status\": \"%s\", \"ip\": \"%s\", \"ssid\": \"%s\", \"info\": \"%s\" },\n";
    char network[150];

    String status = "";
    status = ETH.started() ? "down" : "off";
    if ( ETH.hasIP() ) status = "up";
    sprintf(network, nfmt, "Ethernet", status.c_str(),
      status == "up" ? ETH.localIP().toString().c_str() : "-",
      "-",
      "");
    serverVars += network;

    status = WiFi.STA.started() ? "down" : "off";
    if ( WiFi.STA.hasIP() ) status = "up";
    sprintf(network, nfmt, "WiFi", status.c_str(),
      status == "up" ? WiFi.localIP().toString().c_str() : "-",
      getSSID().c_str(),
      ( String(WiFi.RSSI()) + " dBm" ).c_str());
    serverVars += network;
    //Serial.printf("WiFi.getTxPower() %d\n", WiFi.getTxPower());

    status = WiFi.AP.started() ? "up" : "off";
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
    //       { "id": "0", "status": "down", "name": "Camera 1", "ip": "10.0.4.40", "port": "5678", "type": "0", "transport": "1", "headers": "0" },
    const char cfmt[] = "{ \"id\": \"%d\", \"status\": \"%s\", \"name\": \"%s\", \"ip\": \"%s\", \"port\": \"%d\", \"type\": \"%d\", \"transport\": \"%d\", \"headers\": \"%d\", \"user\": \"%s\" },\n";
    int cameraNumber = 0;
    for ( uint16_t i = 0; i < NUM_CAMERAS; i++ ) {
      // If no switcher than show all potential inputs...
      if ( atemSwitcher.isConnected() && !atemSwitcher.isInputInitialized(i) )
        continue;
      // 0 is an external port input on the ATEM - thats what we want
      if ( atemSwitcher.isConnected() && atemSwitcher.getInputPortType(i) != 0 )
        continue;

      int s = cachedCameraStatus(cameraNumber);   // cache filled by the main loop; no I/O here
      String status = "na";
      if ( s == CAMERA_UP ) status = "up";
      if ( s == CAMERA_DOWN ) status = "down";
      if ( s == CAMERA_OFF ) status = "off";

      String camName = atemSwitcher.getInputShortName(i);
      if ( camName == "" ) camName = "Camera " + String(i);
      char input[220];
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
        settings.cameraType[cameraNumber],
        settings.cameraTransport[cameraNumber],
        settings.cameraHeaders[cameraNumber],
        settings.cameraUser[cameraNumber]);
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
  // Discovered cameras start empty on page load; the list is filled client-side
  // by the /discoverCameras AJAX call when the user clicks "Discover Cameras".
  else if ( var == "DiscoveredCameras" ) return String();

  logi("Unknown replace: %s\n", var.c_str());
  return String();
}

// ---------------------------------------------------------------------------
// Response helpers
// ---------------------------------------------------------------------------

// Broadcast one telemetry frame to every connected /ws client. Called from the
// camera-control loop; keeps PsychicHttp types out of Controller.cpp.
void broadcastTelemetry(const char* msg) {
  ptzWs.sendAll(HTTPD_WS_TYPE_TEXT, (void*)msg, strlen(msg));
}

// Stream a flash-embedded HTML template through PsychicHttp's TemplatePrinter,
// substituting %VAR% via processor(). Streamed in chunks -- no giant heap String
// (this is what removes the multi-second config-page stall). Every literal % in
// config.html is followed by a non-parameter char, so TemplatePrinter re-emits
// them verbatim -- no escaping step needed now that the file is embedded raw.
static esp_err_t sendTemplate(PsychicResponse* response, const char* embeddedHtml) {
  PsychicStreamResponse stream(response, "text/html");
  stream.beginSend();
  TemplatePrinter::start(stream,
    [](Print& out, const char* param) -> bool {
      out.print(processor(String(param)));   // unknown -> "" (consumed), matches old behavior
      return true;
    },
    [embeddedHtml](TemplatePrinter& printer) {
      printer.print(embeddedHtml);   // embedded flash is memory-mapped; read directly
    });
  return stream.endSend();
}

// Serve a gzipped, long-cached static asset embedded in flash.
static esp_err_t sendGzipAsset(PsychicResponse* response, const char* type,
                               const uint8_t* data, size_t len) {
  response->addHeader("Cache-Control", "public, max-age=2678400");
  response->addHeader("Content-Encoding", "gzip");
  return response->send(200, type, data, len);
}

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------

static esp_err_t handleRoot(PsychicRequest* request, PsychicResponse* response) {
  logi("web request for: %s\n", request->uri().c_str());
  return sendTemplate(response, config_html);
}

static esp_err_t handleRestartAndWait(PsychicRequest* request, PsychicResponse* response) {
  if ( RestartMsg == "" ) RestartMsg = "Restart Successful.";
  Serial.println("RESTART - Sending HTML");
  esp_err_t r = sendTemplate(response, restart_html);
  Restart = true;   // the main loop performs the actual reboot
  return r;
}

static esp_err_t handleDiscoverCameras(PsychicRequest* request, PsychicResponse* response) {
  logi("web request for: %s\n", request->uri().c_str());
  const char fmt[] = "{ \"id\": \"%d\", \"name\": \"%s\", \"ip\": \"%s\", \"port\": \"%u\", \"type\": \"%d\", \"transport\": \"%d\", \"headers\": \"%d\" }";

  // Run both discovery mechanisms; each appends into discoveredCameras[],
  // de-duplicated by ip:port. (VISCA-TCP is not discoverable and stays manual.)
  resetDiscovered();
  discoverCameras();        // VISCA-UDP broadcast probe
  discoverOnvifCameras();   // ONVIF WS-Discovery

  String inputs = "[";
  for ( int i = 0; i < numDiscoveredCameras; ++i ) {
    DiscoveredCamera &c = discoveredCameras[i];
    if ( i > 0 ) inputs += ',';
    // VISCA-TCP is raw (no framing); everything else is framed (matches handleSave()).
    int headers = ( c.type == CAM_VISCA && c.transport == CAM_TCP ) ? 0 : 1;
    char input[170];
    sprintf(input, fmt, i, c.name, c.ip.toString().c_str(), c.port, c.type, c.transport, headers);
    inputs += input;
  }
  inputs += "]";
  logi("Discovered %d camera(s): %s", numDiscoveredCameras, inputs.c_str());
  return response->send(200, "application/json", inputs.c_str());
}

// Save new settings from client into EEPROM and restart. Fields are read by name
// (getParam returns null when a field wasn't submitted, so only present values
// change), and the per-camera fields are queried by index.
static esp_err_t handleSave(PsychicRequest* request, PsychicResponse* response) {
  logi("SAVE request for: %s\n", request->uri().c_str());

  PsychicWebParameter* p;
  if ( (p = request->getParam("networkName")) )     strlcpy(settings.ssid, p->value().c_str(), sizeof(settings.ssid));
  if ( (p = request->getParam("networkPassword")) ) strlcpy(settings.psk,  p->value().c_str(), sizeof(settings.psk));
  if ( (p = request->getParam("staticIP")) )        settings.staticIP = ( p->value() == "true" );
  if ( (p = request->getParam("staticIPAddr")) )    settings.staticIPAddr.fromString(p->value());
  if ( (p = request->getParam("staticSubnetMask")) ) settings.staticSubnetMask.fromString(p->value());
  if ( (p = request->getParam("staticGateway")) )   settings.staticGateway.fromString(p->value());
  if ( (p = request->getParam("atemConfigIP")) )    settings.switcherIP.fromString(p->value());

  for ( int i = 0; i < NUM_CAMERAS; i++ ) {
    String n = String(i);
    if ( (p = request->getParam(("camConfigIP" + n).c_str())) )        settings.cameraIP[i].fromString(p->value());
    if ( (p = request->getParam(("camConfigType" + n).c_str())) )      settings.cameraType[i]      = p->value().toInt(); // CAM_VISCA | CAM_ONVIF
    if ( (p = request->getParam(("camConfigTransport" + n).c_str())) ) settings.cameraTransport[i] = p->value().toInt(); // CAM_UDP   | CAM_TCP
    if ( (p = request->getParam(("camConfigPort" + n).c_str())) )      settings.cameraPort[i]      = p->value().toInt();
    if ( (p = request->getParam(("camConfigUser" + n).c_str())) )      strlcpy(settings.cameraUser[i], p->value().c_str(), sizeof(settings.cameraUser[i]));
    // Password is only echoed as blank, so an empty field means "keep existing".
    if ( (p = request->getParam(("camConfigPass" + n).c_str())) && p->value().length() )
                                                                       strlcpy(settings.cameraPass[i], p->value().c_str(), sizeof(settings.cameraPass[i]));
  }

  // VISCA-IP header framing: VISCA over UDP is framed, VISCA over TCP is raw.
  // (ONVIF ignores it.) Derived, not separately configured.
  for ( int c = 0; c <= NUM_CAMERAS; c++ ) {
    settings.cameraHeaders[c] =
      ( settings.cameraType[c] == CAM_VISCA && settings.cameraTransport[c] == CAM_TCP ) ? 0 : 1;
  }

  if ( !InSimulator ) {
    settings.magic = SETTINGS_MAGIC;
    settings.version = SETTINGS_VERSION;
    EEPROM.put(0, settings);
    EEPROM.commit();
  }

  RestartMsg = "Successfully updated settings.";
  return handleRestartAndWait(request, response);
}

static esp_err_t handleErase(PsychicRequest* request, PsychicResponse* response) {
  for ( size_t i = 0; i < sizeof(settings); i++ ) EEPROM.write(i, 255);
  EEPROM.commit();
  RestartMsg = "All Data Erased.";
  return handleRestartAndWait(request, response);
}

static esp_err_t handleNotFound(PsychicRequest* request, PsychicResponse* response) {
  logi("web request for: %s\n", request->uri().c_str());
  return response->send(404, "text/html", "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>PTZ Setup</title></head><body style=\"font-family:Verdana;\"><table bgcolor=\"#777777\"border=\"0\"width=\"100%\"cellpadding=\"1\"style=\"color:#ffffff;font-size:.8em;\"><tr><td><h1>&nbsp PTZ Setup</h1></td></tr></table><br>404 - Page not found</body></html>");
}

// OTA firmware upload: chunked write to Update, then the client polls /restart.
static esp_err_t otaUploadChunk(PsychicRequest* request, const String& filename,
                                uint64_t index, uint8_t* data, size_t len, bool last) {
  g_otaLastChunk = millis();
  if ( index == 0 ) {
    Serial.printf("UPDATE: %s\n", filename.c_str());
    if ( !Update.begin(UPDATE_SIZE_UNKNOWN) ) {
      Update.printError(Serial);
      g_otaActive = false;      // never took over the display; release it
      return ESP_FAIL;          // don't stream the rest of the image into a failed session
    }
    g_otaActive = true;         // the display takes over with the "do not power off" screen
  }
  if ( !g_otaActive ) return ESP_FAIL;   // begin() failed on chunk 0; refuse the remaining chunks
  if ( len && Update.write(data, len) != len ) Update.printError(Serial);
  g_otaBytes = index + len;
  if ( last ) {
    if ( Update.end(true) ) Serial.printf("Update Success: %llu\n", index + len);
    else Update.printError(Serial);
    g_otaActive = false;        // upload finished; /restart reboots, but release the display now
  }
  return ESP_OK;
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

void webSetup() {
  server.config.max_uri_handlers = 24;   // headroom over the ~14 routes below

  server.on("/", HTTP_GET, handleRoot);

  server.on("/bootstrap.min.css", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    return sendGzipAsset(res, "text/css", bootstrap_css_gz, bootstrap_css_gz_end - bootstrap_css_gz); });
  server.on("/headers.css", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    return sendGzipAsset(res, "text/css", headers_css_gz, headers_css_gz_end - headers_css_gz); });
  server.on("/bootstrap.bundle.min.js", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    return sendGzipAsset(res, "text/javascript", bootstrap_js_gz, bootstrap_js_gz_end - bootstrap_js_gz); });
  server.on("/validate-forms.js", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    return sendGzipAsset(res, "text/javascript", validate_js_gz, validate_js_gz_end - validate_js_gz); });

  server.on("/ping", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    return res->send(200, "text/plain", "pong"); });

  server.on("/discoverCameras", HTTP_GET, handleDiscoverCameras);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/restart", HTTP_ANY, handleRestartAndWait);
  server.on("/logData", HTTP_GET, handleLogData);
  server.on("/erase", HTTP_GET, handleErase);

  // OTA firmware upload
  PsychicUploadHandler* updateHandler = new PsychicUploadHandler();
  updateHandler->onUpload(otaUploadChunk);
  updateHandler->onRequest([](PsychicRequest* req, PsychicResponse* res) {
    RestartMsg = String("Firmware Update: ") + ( Update.hasError() ? "FAIL" : "OK" ) + ".";
    // The client's JS calls /restart next, which performs the reboot.
    return res->send(200, "text/plain", "pong");
  });
  server.on("/update", HTTP_POST, updateHandler);

  // Joystick / PTZ telemetry WebSocket, on the same port 80 (was mWebSockets:3000).
  ptzWs.onOpen([](PsychicWebSocketClient* client) {
    logi("ws telemetry client connected: %s", client->remoteIP().toString().c_str()); });
  ptzWs.onFrame([](PsychicWebSocketRequest* req, httpd_ws_frame_t* frame) -> esp_err_t {
    return ESP_OK;   // browser only receives telemetry; inbound frames ignored
  });
  server.on("/ws", &ptzWs);

  server.onNotFound(handleNotFound);
  server.begin();
}

void webLoop() {
  // /save, /erase and /restart set Restart from the async HTTP task; the actual
  // reboot must happen here in loop context. (This flag previously had no consumer,
  // so saving Wi-Fi settings persisted them to EEPROM but never rebooted -- the
  // device kept running in its boot-time mode and never reconnected.) The short
  // delay lets the HTTP response (the "restarting" page) flush to the browser.
  if ( Restart ) {
    logi("Restart requested -- rebooting to apply settings");
    delay(400);
    ESP.restart();
  }

  // PsychicHttp serves requests in its own FreeRTOS task, so there is normally
  // nothing to poll here. The one exception: if an OTA upload dies mid-stream
  // (dropped client, network glitch), the upload callback stops firing and
  // g_otaActive would otherwise stay latched forever, wedging the display on the
  // "do not power off" screen. Detect the stall here and abort the update.
  #define OTA_STALL_MS 15000
  if ( g_otaActive && ( millis() - g_otaLastChunk > OTA_STALL_MS ) ) {
    logw("OTA upload stalled (%lu ms with no data); aborting", millis() - g_otaLastChunk);
    Update.abort();
    g_otaActive = false;
  }
}
