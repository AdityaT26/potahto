// Device simulator: REAL Proto.cpp + Settings.cpp + ArduinoJson, fake hardware.
//
// stdin : "<mode>\t<pin>\t<json>\n"  mode = serial|http
// stdout: one JSON response line per request
#include "Multitool.h"
#include "Proto.h"
#include "HidPriv.h"
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <WiFi.h>
WiFiClass WiFi;
EspClass ESP;
const App APPS[APP_COUNT] = {
  {"Potahto",0,0,nullptr,nullptr,nullptr},{"Battery",0,0,nullptr,nullptr,nullptr},{"IR Remote",0,0,nullptr,nullptr,nullptr},
  {"Motion",0,0,nullptr,nullptr,nullptr},{"Light",0,0,nullptr,nullptr,nullptr},{"Compass",0,0,nullptr,nullptr,nullptr},
  {"Scale",0,0,nullptr,nullptr,nullptr},{"Air Mouse",0,0,nullptr,nullptr,nullptr},{"Tripwire",0,0,nullptr,nullptr,nullptr},
  {"Games",0,0,nullptr,nullptr,nullptr},{"Settings",0,0,nullptr,nullptr,nullptr},{"Connect",0,0,nullptr,nullptr,nullptr}};
uint8_t currentApp() { return 0; }
uint8_t battPercent() { return 73; }
float battVoltage() { return 3.95f; }
bool battCharging() { return false; }
static bool ap = false; static int scanState = -2; static uint32_t scanAt = 0;
const char* netStateName() { return "Online"; }
NetState netState() { return NET_ONLINE; } bool netOnline() { return true; }
int netRefs = 0; void netAcquire() { netRefs++; } void netRelease() { netRefs--; }
bool mpuRead(Vec3f& a, Vec3f& g, float& t) { a = {0,0,9.8f}; g = {0,0,0}; t = 31.5f; return true; }
extern std::vector<std::pair<std::string, std::string>> g_mqttPub; extern std::vector<std::string> g_httpLog; extern std::mutex g_httpLogM;
extern std::string g_mqttUri, g_mqttLwt; void hostMqttInject(const char*, const char*);
const char* netSsid() { return "HomeNet"; }
const char* netIp() { return "192.168.1.42"; }
int netRssi() { return -58; }
const char* netHostname() { return "potahto"; }
bool netSetupAPActive() { return ap; }
void netSetupAP(bool on) { ap = on; }
const char* netApSsid() { return "Potahto-EEFF"; }
void netScanStart() { scanState = -1; scanAt = g_fakeMillis; }
static NetAP aps[5] = {{"HomeNet",-48,6,3,{}},{"Neighbour 5G",-71,36,3,{}},{"CafeFree",-80,1,0,{}},{"HomeNet",-60,11,3,{}},{"CampusNet",-66,1,5,{}}};
const NetAP* netScanResults(int* n) { if (scanState == -1 && g_fakeMillis - scanAt > 1500) scanState = 5; *n = scanState; return aps; }
int reconnects = 0;
void netReconnect() { reconnects++; }
bool netTimeValid() { return true; }
bool mpuPresent() { return true; } bool compassPresent() { return true; } bool tofPresent() { return false; }
std::string lastToast;
int gamesCount() { return 3; }
const char* gameName(int i) { static const char* n[] = {"Snake","Tetris","Dungeon"}; return n[i]; }
const char* gameHsKey(int i) { static const char* k[] = {"hsSnake","hsTetris","hsDungeon"}; return k[i]; }
uint32_t hiscoreGet(const char* k) { return prefs.getUInt(k, 0); }
void hiscoreSet(const char* k, uint32_t v) { prefs.putUInt(k, v); }
// ---- fake file store + HID (real Macros.cpp / HidMacro.cpp on top)
static std::map<std::string, std::string> fs;
bool storeOk() { return true; }
bool storeRead(const char* p, String& out, size_t) { auto it = fs.find(p); if (it == fs.end()) return false; out = String(it->second.c_str()); return true; }
bool storeWrite(const char* p, const char* d, size_t n) { fs[p] = std::string(d, n); return true; }
bool storeRemove(const char* p) { return fs.erase(p) > 0; }
static std::string typed;
static const char* US = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
void hidxKbd(uint8_t m, uint8_t u) {
  if (!u) return;
  if (u == 0x28) { typed += "\\n"; return; }
  for (const char* c = US; *c; c++) { /* reverse-map via the real parser would be overkill */ }
  char b[24]; snprintf(b, sizeof b, "<%02x:%02x>", m, u); typed += b;
}
void hidxCons(uint16_t u) { if (u) { char b[16]; snprintf(b, sizeof b, "<media %x>", u); typed += b; } }
uint8_t hidxButtons() { return 0; } bool hidxIsUsb() { return true; }
bool hidReady() { return true; } bool hidUsbMounted() { return true; } bool hidBleStarted() { return true; } bool hidBleConnected() { return false; }
const char* hidRouteName() { return "USB"; } void hidBleStart() {} void hidUnpairAll() { lastToast = "unpaired"; }
void hidMouseButtons(uint8_t) {} void hidMouseMove(int, int, int, int) {} void hidReleaseAll() {}
void mpuCalibrateGyro() { S.gyroBias[0] = 0.5f; } void mpuLevelHere() {}
// ---- wireless toolkit fakes (Wireless.cpp needs NimBLE/lwip)
static std::string wolJson = "[{\"name\":\"Desktop\",\"mac\":\"A8:A1:59:12:34:56\"}]";
static int docPolls = 0;
static void wirelessFakes() {
  protoRegister("ble.scan", [](JsonObjectConst, JsonObject r, ProtoCtx&) { r["scanning"] = true; return true; });
  protoRegister("ble.list", [](JsonObjectConst, JsonObject r, ProtoCtx&) {
    r["scanning"] = true; JsonArray a = r["devices"].to<JsonArray>();
    const char* n[] = {"Buds Pro", "", "Mi Band 7", ""}; const char* v[] = {"Samsung", "Apple", "Huami", "Apple"}; int rs[] = {-52, -63, -70, -78};
    for (int i = 0; i < 4; i++) { JsonObject o = a.add<JsonObject>(); o["addr"] = "C1:02:03:04:05:06"; o["name"] = n[i]; o["vendor"] = v[i]; o["rssi"] = rs[i]; if (i == 3) o["kind"] = "iBeacon"; o["age"] = 1; }
    return true; });
  protoRegister("lan.scan", [](JsonObjectConst, JsonObject, ProtoCtx&) { return true; });
  protoRegister("lan.list", [](JsonObjectConst, JsonObject r, ProtoCtx&) {
    r["scanning"] = false; r["progress"] = 100; JsonArray a = r["hosts"].to<JsonArray>();
    const char* h[] = {"desktop", "HP-LaserJet", "homeassistant"}; const char* t[] = {"SMB", "Printer", "Home Asst"}; const char* ip[] = {"192.168.1.42", "192.168.1.10", "192.168.1.5"};
    for (int i = 0; i < 3; i++) { JsonObject o = a.add<JsonObject>(); o["host"] = h[i]; o["type"] = t[i]; o["ip"] = ip[i]; o["port"] = 80; }
    return true; });
  protoRegister("doctor.run", [](JsonObjectConst, JsonObject, ProtoCtx&) { docPolls = 0; return true; });
  protoRegister("doctor.status", [](JsonObjectConst, JsonObject r, ProtoCtx&) {
    docPolls++; r["running"] = docPolls < 3; JsonArray a = r["steps"].to<JsonArray>();
    const char* n[] = {"WiFi link", "Gateway", "DNS", "Internet", "Latency"}; const char* note[] = {"HomeNet ch6 -48 dBm", "3 ms", "21 ms", "HTTP 204", "avg 40 ms, jitter 38 ms"};
    for (int i = 0; i < 5; i++) { JsonObject o = a.add<JsonObject>(); o["name"] = n[i]; bool done = i < docPolls * 2; o["state"] = !done ? "pending" : i == 4 ? "warn" : "ok"; o["note"] = done ? note[i] : ""; }
    return true; });
  protoRegister("wol.get", [](JsonObjectConst, JsonObject r, ProtoCtx&) { JsonDocument d; deserializeJson(d, wolJson); r["devices"] = d.as<JsonArrayConst>(); r["max"] = 6; return true; });
  protoRegister("wol.set", [](JsonObjectConst q, JsonObject r, ProtoCtx&) {
    for (JsonObjectConst o : q["devices"].as<JsonArrayConst>()) { String m = o["mac"] | ""; if (m.length() != 17) { r["err"] = String("bad MAC for ") + (const char*)(o["name"] | "?"); return false; } }
    String s; serializeJson(q["devices"], s); wolJson = s.c_str(); return true; });
  protoRegister("wol.send", [](JsonObjectConst q, JsonObject r, ProtoCtx&) { std::cerr << "[wol] " << (int)(q["i"] | -1) << "\n"; return true; });
}

// ---- 1.3.0 fakes: power, watch, BLE link, pointer/event hooks
static bool g_scr = true, g_trip = false, g_guard = false;
bool powerScreenOn() { return g_scr; }
void powerScreenOff(bool) { g_scr = false; }
void powerWake() { g_scr = true; }
void powerPeek(uint32_t) { g_scr = true; }
void powerLockNow() { g_scr = false; }
bool powerLedIdle() { return false; }
[[noreturn]] void powerDeepSleep() { std::cerr << "[deep sleep]\n"; exit(0); }
bool tripArmed() { return g_trip; } bool guardArmed() { return g_guard; } bool watchBusy() { return g_trip || g_guard; }
const char* blePeerName() { return ""; }
void protoEmitBle(const char*) {}
void hidxPointer(uint8_t m) { typed += "<pointer>"; }
void hidxEvent(const char* k, const char* a) { std::cerr << "[event] " << k << " " << a << "\n"; JsonDocument d; d["event"] = k; d["cmd"] = a; protoEmit(d.as<JsonObjectConst>()); }
void macroAppChanged(const char*) {}
bool netEncEnterprise(uint8_t e) { return e == 5; }
bool netEncOpen(uint8_t e) { return e == 0; }
static void watchFake() {
  protoRegister("watch", [](JsonObjectConst q, JsonObject r, ProtoCtx&) {
    if (!q["trip"].isNull()) g_trip = q["trip"] | false;
    if (!q["guard"].isNull()) g_guard = q["guard"] | false;
    r["trip"] = g_trip; r["tripped"] = false; r["guard"] = g_guard; r["guardState"] = g_guard ? "present" : "off";
    r["last"] = ""; r["mm"] = 734; r["tof"] = true; return true; });
}

int main() {
  settingsBegin();
  strcpy(S.webPin, "123456");
  hiscoreSet("hsSnake", 42); hiscoreSet("hsDungeon", 7);
  protoBegin();
  macrosBegin();
  notifyBegin();
  deskBegin();
  iotBegin();
  wirelessFakes();
  watchFake();
  musicLibBegin();
  std::string line;
  while (std::getline(std::cin, line)) {
    g_fakeMillis += 800;
    size_t a = line.find('\t'), b = line.find('\t', a + 1);
    if (line.rfind("#mqtt ", 0) == 0) { size_t sp = line.find(' ', 6); hostMqttInject(line.substr(6, sp - 6).c_str(), line.substr(sp + 1).c_str()); g_fakeMillis += 50; iotLoop(); notifyLoop(); std::cout << "{\"ok\":true}\n" << std::flush; continue; }
    if (line == "#loop") { for (int i = 0; i < 5; i++) { g_fakeMillis += 50; iotLoop(); deskLoop(); notifyLoop(); } std::cout << "{\"ok\":true}\n" << std::flush; continue; }
    if (line == "#mqttdump") { std::string o = "{\"uri\":\"" + g_mqttUri + "\",\"lwt\":\"" + g_mqttLwt + "\",\"pubs\":["; bool f = true;
      for (auto& p : g_mqttPub) { String esc; for (char c : p.second) { if (c == '"' || c == '\\') esc += '\\'; esc += c; } o += std::string(f ? "" : ",") + "[\"" + p.first + "\",\"" + esc.c_str() + "\"]"; f = false; }
      o += "]}"; g_mqttPub.clear(); std::cout << o << "\n" << std::flush; continue; }
    if (line == "#hooks") { std::this_thread::sleep_for(std::chrono::milliseconds(150)); std::lock_guard<std::mutex> l(g_httpLogM); std::string o = "{\"hooks\":["; bool f = true;
      for (auto& h : g_httpLog) { String esc; for (char c : h) { if (c == '"' || c == '\\') esc += '\\'; esc += c; } o += std::string(f ? "" : ",") + "\"" + esc.c_str() + "\""; f = false; }
      o += "]}"; g_httpLog.clear(); std::cout << o << "\n" << std::flush; continue; }
    if (a == std::string::npos || b == std::string::npos) { std::cout << "{}\n" << std::flush; continue; }
    std::string mode = line.substr(0, a), pin = line.substr(a + 1, b - a - 1), body = line.substr(b + 1);
    ProtoCtx ctx{ mode == "serial" ? PV_SERIAL : PV_HTTP, mode == "serial" || (!pin.empty() && protoPinCheck(pin.c_str())) };
    String out;
    protoHandle(body.c_str(), body.size(), out, ctx);
    settingsTick(); iotLoop(); deskLoop();
    for (int i = 0; i < 4000 && hidBusy(); i++) { g_fakeMillis++; hidMacroTick(); }
    if (!typed.empty()) { std::cerr << "[typed] " << typed << "\n"; typed.clear(); }
    std::cout << out.c_str() << "\n" << std::flush;
  }
}
