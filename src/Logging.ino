#include <Arduino.h>
#include <stdarg.h>
#include <ESPAsyncWebServer.h>


#define LOG_SIZE 256


static uint8_t count = 0;
static uint8_t ptr = 0;
static char lbuf[LOG_SIZE][256];

static void handleLog(AsyncWebServerRequest *request) {
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
  html += "<div id=\"logData\"></div>\n";

  html += "</body></html>";
  request->send(200, "text/html", html);
}

static void handleLogData(AsyncWebServerRequest *request) {
  uint8_t p = ptr;
  String html = "";
  for (int i = 0; i < count; i++, p--) {
    html += lbuf[p];
    html += "<br/>";
  }
  request->send(200, "text/html", html);
}

static void logi(const char *fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if (count < LOG_SIZE - 1) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(lbuf[ptr], fmt, args);
  Serial.print(lbuf[ptr]);
  va_end(args);
}

static void logd(const char *fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if (count < LOG_SIZE - 1) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(lbuf[ptr], fmt, args);
  Serial.print(lbuf[ptr]);
  va_end(args);
}

static void logw(const char *fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if (count < LOG_SIZE - 1) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(lbuf[ptr], fmt, args);
  Serial.print(lbuf[ptr]);
  va_end(args);
}
static void loge(const char *fmt, ...) {
  va_list args;

  ptr += 1;  // should wrap around automatically
  if (count < LOG_SIZE - 1) {
    count++;
  }
  va_start(args, fmt);
  vsprintf(lbuf[ptr], fmt, args);
  Serial.print(lbuf[ptr]);
  va_end(args);
}
