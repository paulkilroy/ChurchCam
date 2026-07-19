#include <Arduino.h>
#include <stdarg.h>
#include <PsychicHttp.h>

#include "globals.h"

#define LOG_SIZE 32

static uint8_t count = 0;
static uint8_t ptr = 0;
struct LogItem LogItems[LOG_SIZE];

#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARN 3
#define LOG_ERROR 4

/*
static void handleLog(AsyncWebServerRequest* request) {
  String html = "<!DOCTYPE html><html><head><meta charset=\"ASCII\"><meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\"><title>Debug Logs</title>";
  html += "<script>\n";
  html += "\n";
  html += "setInterval(function() {\n";
  html += "  // Call a function repetatively with 2 Second interval\n";
  html += "  getData();\n";
  html += "}, 2000); //2000mSeconds update rate\n";
  html += "\n";
  html += "function getData() {\n";
  html += "  var xhttp = new XMLHttpRequest();\n";
  html += "  xhttp.onreadystatechange = function() {\n";
  html += "    if (this.readyState == 4 && this.status == 200) {\n";
  html += "      document.getElementById(\"logData\").innerHTML =\n";
  html += "      this.responseText;\n";
  html += "    }\n";
  html += "  };\n";
  html += "  xhttp.open(\"GET\", \"logData\", true);\n";
  html += "  xhttp.send();\n";
  html += "}\n";
  html += "</script>";
  html += "</head><style>a{color:#0F79E0}</style><body style=\"font-family:Verdana;white-space:nowrap;\">";
  html += "<h1>PTZ Logs</h1>\n";
  html += "<div id=\"logData\"></div>\n";
  html += "</body></html>";
  request->send(200, "text/html", html);
}
*/

// The i-th most recent log line (0 = newest). Indices are reduced modulo
// LOG_SIZE so a wrapped ptr never reads out of bounds.
struct LogItem getLogItem(uint8_t i) {
  int idx = ((int)ptr - (i % LOG_SIZE) + LOG_SIZE) % LOG_SIZE;
  return LogItems[idx];
}

esp_err_t handleLogData(PsychicRequest* request, PsychicResponse* response) {
  String html = "";
  for ( int i = 0; i < count; i++ ) {
    struct LogItem item = getLogItem(i);   // masked ring read (newest first)
    String  cls = "alert-primary";
    switch (item.type) {
      case LOG_INFO: cls = "alert-info"; break;
      case LOG_DEBUG: cls = "alert-light"; break;
      case LOG_WARN: cls = "alert-warning"; break;
      case LOG_ERROR: cls = "alert-danger"; break;
    }
    html += "<div class=\"alert " + cls + " mb-1 py-1\" role=\"alert\">";
    int mills = (int) (item.time % 1000);
    int seconds = (int) (item.time / 1000) % 60 ;
    int minutes = (int) ((item.time / (1000*60)) % 60);
    int hours   = (int) ((item.time / (1000*60*60)) % 24);
    html += String(hours) + ":" + String(minutes) + ":" + String(seconds) + "." + String(mills);
    html += " - ";
    html += item.buf;
    html += "</div>";
  }
  return response->send(200, "text/html", html.c_str());
}

// Append one formatted line to the log ring. Formatting is done on the caller's
// stack; only the pointer bump + slot claim run under a brief critical section,
// so concurrent callers (main loop, WiFi/ETH event task, PsychicHttp task) can't
// corrupt ptr/count or overwrite each other. ptr is kept in [0, LOG_SIZE) so the
// uint8_t counter can never index past the array.
static portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

static void logAppend(int type, bool toSerial, const char* fmt, va_list args) {
  char buf[sizeof(LogItems[0].buf)];
  vsnprintf(buf, sizeof(buf), fmt, args);

  portENTER_CRITICAL(&logMux);
  ptr = (ptr + 1) % LOG_SIZE;
  if ( count < LOG_SIZE - 1 ) count++;
  uint8_t slot = ptr;
  portEXIT_CRITICAL(&logMux);

  strlcpy(LogItems[slot].buf, buf, sizeof(LogItems[slot].buf));
  LogItems[slot].type = type;
  LogItems[slot].time = millis();

  if ( toSerial ) Serial.print(buf);
}

void display_i(const char* fmt, ...) { va_list a; va_start(a, fmt); logAppend(LOG_INFO,  false, fmt, a); va_end(a); }
void logd(const char* fmt, ...)      { va_list a; va_start(a, fmt); logAppend(LOG_DEBUG, true,  fmt, a); va_end(a); }
void logw(const char* fmt, ...)      { va_list a; va_start(a, fmt); logAppend(LOG_WARN,  true,  fmt, a); va_end(a); }
void loge(const char* fmt, ...)      { va_list a; va_start(a, fmt); logAppend(LOG_ERROR, true,  fmt, a); va_end(a); }
