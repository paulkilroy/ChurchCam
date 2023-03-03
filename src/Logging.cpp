#include <Arduino.h>
#include <stdarg.h>

#include "globals.h"

#define LOG_SIZE 256

static uint8_t count = 0;
static uint8_t ptr = 0;
struct LogItem LogItems[256];

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

struct LogItem getLogItem(uint8_t i) {
  return LogItems[(uint8_t)(ptr-i)];
}

void handleLogData() {
  uint8_t p = ptr;
  String html = "";
  for ( int i = 0; i < count; i++, p-- ) {
    String  cls = "alert-primary";
    switch (LogItems[p].type) {
      case LOG_INFO: cls = "alert-info"; break;
      case LOG_DEBUG: cls = "alert-light"; break;
      case LOG_WARN: cls = "alert-warning"; break;
      case LOG_ERROR: cls = "alert-danger"; break;
    }
    html += "<div class=\"alert " + cls + " mb-1 py-1\" role=\"alert\">";
    int mills = (int) (LogItems[p].time % 1000);
    int seconds = (int) (LogItems[p].time / 1000) % 60 ;
    int minutes = (int) ((LogItems[p].time / (1000*60)) % 60);
    int hours   = (int) ((LogItems[p].time / (1000*60*60)) % 24);
    html += String(hours) + ":" + String(minutes) + ":" + String(seconds) + "." + String(mills);
    html += " - ";
    html += LogItems[p].buf;
    html += "</div>";
  }
  srvr.send(200, "text/html", html);
}

void display_i(const char* fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if ( count < LOG_SIZE - 1 ) {
    count++;
  }

  va_start(args, fmt);
  vsprintf(LogItems[ptr].buf, fmt, args);
  LogItems[ptr].type = LOG_INFO;
  LogItems[ptr].time = millis();

  //Serial.print(LogItems[ptr].buf);
  va_end(args);
}

void logd(const char* fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if ( count < LOG_SIZE - 1 ) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(LogItems[ptr].buf, fmt, args);
  LogItems[ptr].type = LOG_DEBUG;
  LogItems[ptr].time = millis();
  Serial.print(LogItems[ptr].buf);
  va_end(args);
}

void logw(const char* fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if ( count < LOG_SIZE - 1 ) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(LogItems[ptr].buf, fmt, args);
  LogItems[ptr].type = LOG_WARN;
  LogItems[ptr].time = millis();
  Serial.print(LogItems[ptr].buf);
  va_end(args);
}

void loge(const char* fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if ( count < LOG_SIZE - 1 ) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(LogItems[ptr].buf, fmt, args);
  LogItems[ptr].type = LOG_ERROR;
  LogItems[ptr].time = millis();

  Serial.print(LogItems[ptr].buf);
  va_end(args);
}
