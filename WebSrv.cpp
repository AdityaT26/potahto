// HTTP server: web app, JSON API, firmware upload, captive portal
//   GET  /            web app (gzipped, embedded in firmware: WebApp.h)
//   POST /api         config protocol (Proto.h), PIN in "X-Pin" header
//   POST /update      firmware .bin upload (multipart), PIN in "X-Pin" or ?pin=
//   *                 in setup-AP mode every unknown URL redirects to the app,
//                     which makes phones/PCs pop up the WiFi page on join.
// CORS is open so a locally opened copy of the web app can talk to the device;
// every state-changing call still needs the PIN.
#include "Multitool.h"
#include "Proto.h"
#include <WebServer.h>
#include <Update.h>
#include "WebApp.h"

#define API_BODY_MAX  (24 * 1024)

static WebServer* s_srv = nullptr;
static bool   s_running = false;
static bool   s_upAuthed = false, s_upOk = false;
static String s_upErr;
static size_t s_upDone = 0, s_upTotal = 0, s_upLastDraw = 0;

static void cors() {
  s_srv->sendHeader("Access-Control-Allow-Origin", "*");
  s_srv->sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Pin");
  s_srv->sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  s_srv->sendHeader("Access-Control-Allow-Private-Network", "true");
}
static String reqPin() {
  String p = s_srv->header("X-Pin");
  if (!p.length()) p = s_srv->arg("pin");
  return p;
}
static void sendApp() {
  s_srv->sendHeader("Content-Encoding", "gzip");
  s_srv->sendHeader("Cache-Control", "no-cache");
  s_srv->send_P(200, "text/html", (const char*)WEBAPP_GZ, WEBAPP_GZ_LEN);
}
static void redirectToApp() {
  s_srv->sendHeader("Location", String("http://") + netIp() + "/", true);
  s_srv->send(302, "text/plain", "");
}
static void handleOptions() { cors(); s_srv->send(204); }

static void handleApi() {
  cors();
  const String& body = s_srv->arg("plain");
  if (body.length() == 0 || body.length() > API_BODY_MAX) {
    s_srv->send(400, "application/json", "{\"ok\":false,\"err\":\"bad body\"}");
    return;
  }
  String pin = reqPin();
  ProtoCtx ctx{ PV_HTTP, pin.length() > 0 && protoPinCheck(pin.c_str()) };
  String out;
  protoHandle(body.c_str(), body.length(), out, ctx);
  s_srv->send(200, "application/json", out);
}

static void handleUpload() {
  HTTPUpload& up = s_srv->upload();
  if (up.status == UPLOAD_FILE_START) {
    s_upOk = false; s_upErr = ""; s_upDone = 0; s_upLastDraw = 0;
    String pin = reqPin();
    s_upAuthed = pin.length() > 0 && protoPinCheck(pin.c_str());
    if (!s_upAuthed) { s_upErr = "auth"; return; }
    s_upTotal = s_srv->header("Content-Length").toInt();
    otaSetActive(true);
    otaDrawProgress(up.filename.c_str(), 0, s_upTotal);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) s_upErr = Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!s_upAuthed || s_upErr.length()) return;
    if (s_upDone == 0 && up.currentSize > 0 && up.buf[0] != 0xE9) {      // ESP image magic
      s_upErr = "not an ESP32 firmware .bin"; Update.abort(); return;
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) { s_upErr = Update.errorString(); Update.abort(); return; }
    s_upDone += up.currentSize;
    if (s_upDone - s_upLastDraw > 24 * 1024) { s_upLastDraw = s_upDone; otaDrawProgress("Receiving...", s_upDone, s_upTotal); }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!s_upAuthed || s_upErr.length()) return;
    if (Update.end(true)) s_upOk = true;                 // also validates chip + checksum
    else s_upErr = Update.errorString();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort(); s_upErr = "aborted";
  }
}
static void handleUploadDone() {
  cors();
  if (s_upOk) {
    otaDrawProgress("Done - rebooting", 1, 1);
    s_srv->send(200, "application/json", "{\"ok\":true}");
    settingsSave();
    delay(400);
    ESP.restart();
  } else {
    otaSetActive(false);
    if (s_upErr.length() && s_upErr != "auth") { toast("Update failed"); fbError(); }
    String e = s_upErr.length() ? s_upErr : String("no file");
    e.replace("\"", "'");
    s_srv->send(s_upErr == "auth" ? 403 : 400, "application/json", "{\"ok\":false,\"err\":\"" + e + "\"}");
  }
}

void webStart() {
  if (!s_srv) {
    s_srv = new WebServer(80);
    static const char* HDRS[] = { "X-Pin", "Content-Length" };
    s_srv->collectHeaders(HDRS, 2);
    s_srv->on("/", HTTP_GET, sendApp);
    s_srv->on("/index.html", HTTP_GET, sendApp);
    s_srv->on("/api", HTTP_POST, handleApi);
    s_srv->on("/api", HTTP_OPTIONS, handleOptions);
    s_srv->on("/update", HTTP_POST, handleUploadDone, handleUpload);
    s_srv->on("/update", HTTP_OPTIONS, handleOptions);
    // OS captive-portal probes -> our page while in setup mode
    for (const char* u : { "/generate_204", "/gen_204", "/hotspot-detect.html",
                           "/connecttest.txt", "/ncsi.txt", "/redirect", "/fwlink" })
      s_srv->on(u, HTTP_GET, []() { if (netSetupAPActive()) redirectToApp(); else s_srv->send(404, "text/plain", "not found"); });
    s_srv->onNotFound([]() {
      if (netSetupAPActive()) redirectToApp();
      else s_srv->send(404, "text/plain", "not found");
    });
  }
  if (!s_running) { s_srv->begin(); s_running = true; }
}
void webStop() {
  if (s_srv && s_running) { s_srv->stop(); s_running = false; }
}
void webLoop() {
  if (s_srv && s_running) s_srv->handleClient();
}
