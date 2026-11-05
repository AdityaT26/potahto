// Virtual Potahto: real firmware UI code + real U8g2, fake radio/sensors.
//
// usage: oled_sim scripts/<name>.script   (see _dev/README.md)
#include "Multitool.h"
#include "Proto.h"
#include "HidPriv.h"
#include <fstream>
#include <sstream>
#include <string>
#include <map>
uint32_t g_fakeMillis = 1000;
uint32_t g_fakeAdcMv = 1980;
uint8_t g_pinLevel[64];
static struct PinInit { PinInit() { memset(g_pinLevel, 1, sizeof g_pinLevel); } } s_pinInit;
HWCDC Serial;
TwoWire Wire;
EspClass ESP;
std::map<std::string, std::map<std::string, NvsVal>> g_nvs;
int g_nvsWrites = 0;
extern "C" int potahto_fake_settimeofday(const void*, const void*) { return 0; }
extern "C" uint8_t u8x8_byte_arduino_hw_i2c(u8x8_t*, uint8_t, uint8_t, void*) { return 1; }
extern "C" uint8_t u8x8_gpio_and_delay_arduino(u8x8_t*, uint8_t, uint8_t, void*) { return 1; }

void setup(); void loop();
// ---- fake network
static NetState fnState = NET_OFF; static bool fnAp = false; static int fnRefs = 0;
void netBegin() {} void netLoop() {}
static int fnSlot = -1;
void netAcquire() { fnRefs++; } void netRelease() { fnRefs--; }
void netReconnect() {}
void netSetupAP(bool on) { fnAp = on; }
bool netSetupAPActive() { return fnAp; }
NetState netState() { return fnState; }
bool netOnline() { return fnState == NET_ONLINE; }
const char* netStateName() { static const char* n[] = {"Off","Scanning","Connecting","Online","No network saved","Not found"}; return n[fnState]; }
const char* netSsid() { return fnState == NET_ONLINE ? "HomeNet" : ""; }
const char* netIp() { return fnState == NET_ONLINE ? "192.168.1.42" : (fnAp ? "192.168.4.1" : ""); }
int netRssi() { return -58; }
const char* netHostname() { return "potahto"; }
const char* netApSsid() { return "Potahto-EEFF"; }
const char* netApPass() { static char p[12]; snprintf(p, sizeof p, "pt%s", S.webPin); return p; }
void netScanStart() {}
static NetAP fakeAps[] = {
  {"HomeNet",-48,6,3,{0x10,0x20,0x30,0x40,0x50,0x60}}, {"HomeNet-5G",-61,36,3,{0x10,0x20,0x30,0x40,0x50,0x61}},
  {"Sharma_2.4",-67,1,3,{1,2,3,4,5,6}}, {"JioFiber-x7",-72,11,3,{2,2,3,4,5,6}}, {"Airtel_Zerotouch",-79,6,3,{3,2,3,4,5,6}},
  {"Cafe Guest",-85,11,0,{4,2,3,4,5,6}}, {"DIRECT-HP-Print",-88,6,3,{5,2,3,4,5,6}} };
const NetAP* netScanResults(int* n) { *n = 7; return fakeAps; }
void netExternalScan(bool) {}
const uint8_t* netBssid() { return fnState == NET_ONLINE ? fakeAps[0].bssid : nullptr; }
int netChannel() { return fnState == NET_ONLINE ? 6 : 0; }
// ---- wireless toolkit fakes
static bool fHunt = false; void wirelessBegin() {} void wirelessLoop() {}
void hunterStart(const uint8_t*, uint8_t, const char*) { fHunt = true; } void hunterStop() { fHunt = false; } bool hunterActive() { return fHunt; }
int hunterRssi() { return -55 + (int)((millis() / 300) % 9) - 4; } const char* hunterSsid() { return "HomeNet"; }
static BleDev fBle[] = {
  {{0xc1,2,3,4,5,6},"Buds Pro",-52,-52,0x0075,0,0}, {{0xd2,2,3,4,5,6},"",-63,-63,0x004C,0,0},
  {{0xe3,2,3,4,5,6},"Mi Band 7",-70,-70,0x0157,0,0}, {{0xf4,2,3,4,5,6},"",-78,-78,0x004C,BK_IBEACON,0},
  {{0xa5,2,3,4,5,6},"LE-Keyboard",-84,-84,0x0006,0,0} };
void bleScanStart() {} void bleScanStop() {} bool bleScanning() { return true; }
int bleSnapshot(BleDev* o, int m) { int n = m < 5 ? m : 5; for (int i = 0; i < n; i++) { o[i] = fBle[i]; o[i].seen = millis(); } return n; }
bool bleLookup(const uint8_t a[6], BleDev& o) { for (auto& d : fBle) if (!memcmp(d.addr, a, 6)) { o = d; o.seen = millis(); o.rssi = -58 + (int)((millis()/400)%7) - 3; o.rssiAvg = o.rssi; return true; } return false; }
const char* bleCompanyName(uint16_t id) { return id == 0x004C ? "Apple" : id == 0x0075 ? "Samsung" : id == 0x0157 ? "Huami" : id == 6 ? "Microsoft" : ""; }
const char* bleKindName(uint8_t k) { static const char* n[] = {"","iBeacon","Eddystone","Find My"}; return n[k]; }
static LanHost fLan[] = { {"desktop",0x2A01A8C0,445,0}, {"HP-LaserJet",0x0A01A8C0,631,1}, {"homeassistant",0x0501A8C0,8123,2}, {"Living Room TV",0x1401A8C0,8009,3} };
void lanScanStart() {} bool lanScanning() { return false; } int lanProgress() { return 100; }
int lanSnapshot(LanHost* o, int m) { int n = m < 4 ? m : 4; memcpy(o, fLan, n * sizeof(LanHost)); return n; }
const char* lanServiceName(uint8_t i) { static const char* n[] = {"SMB","Printer","Home Asst","Cast"}; return i < 4 ? n[i] : "?"; }
static DocResult fDoc[DS_COUNT] = { {1,-48,"HomeNet ch6"}, {1,3,"192.168.1.1"}, {1,21,"8.8.8.8"}, {1,0,"HTTP 204"}, {2,140,"jitter 38 ms"} };
void doctorRun() {} bool doctorRunning() { return false; } const DocResult* doctorResults() { return fDoc; }
const char* doctorStepName(uint8_t i) { static const char* n[] = {"WiFi link","Gateway","DNS","Internet","Latency"}; return n[i]; }
int wolCount() { return 2; } const char* wolName(int i) { return i ? "Media PC" : "Desktop"; } bool wolSend(int) { return true; }
bool otaActive() { return false; } void otaSetActive(bool) {}
void otaDrawProgress(const char*, uint32_t, uint32_t) {}

// ---- fake HID transports + store + proto registry
static bool fakeReady = true;
void hidxKbd(uint8_t, uint8_t) {} void hidxCons(uint16_t) {} uint8_t hidxButtons() { return 0; } bool hidxIsUsb() { return true; }
static bool fakeBle = false; static uint8_t fakeRoute = HR_USB;
bool hidReady() { return fakeReady; } bool hidUsbMounted() { return fakeRoute == HR_USB; } bool hidBleStarted() { return true; } bool hidBleConnected() { return fakeBle; }
const char* hidRouteName() { return !fakeReady ? "BT: waiting for PC" : fakeRoute == HR_USB ? "USB" : "Bluetooth"; } void hidBleStart() {} void hidUnpairAll() {}
void hidToggleRoute() { fakeRoute = fakeRoute == HR_USB ? HR_BLE : HR_USB; } uint8_t hidRoute() { return fakeRoute; }
int hidBondCount() { return 1; } void hidBleAdvertise() {} bool hidBleAdvertising() { return true; }
void hidxPointer(uint8_t m) { pointerStart(m, m == PTR_DRAG ? "Drag to select" : "Pointer"); }
void hidxEvent(const char* k, const char* a) { printf("[event] %s %s\n", k, a); }
void hidRouteOverride(uint8_t) {} void hidBegin() {} void hidLoop() { hidMacroTick(); }
void hidMouseButtons(uint8_t) {} void hidMousePress(uint8_t) {} void hidMouseRelease(uint8_t) {}
void hidMouseMove(int, int, int, int) {} void hidReleaseAll() {} void hidKeysDown(uint8_t, uint8_t) {}
static std::map<std::string, std::string> fs;
bool storeBegin() { return true; } bool storeOk() { return true; }
bool storeRead(const char* p, String& out, size_t) { auto it = fs.find(p); if (it == fs.end()) return false; out = String(it->second.c_str()); return true; }
bool storeWrite(const char* p, const char* d, size_t n) { fs[p] = std::string(d, n); return true; } bool storeRemove(const char* p) { return fs.erase(p) > 0; }
size_t storeFreeBytes() { return 0; }
static std::map<std::string, ProtoFn> s_handlers;
void protoRegister(const char* c, ProtoFn f, bool) { s_handlers[c] = f; }
static void protoCall(const std::string& json) {
  JsonDocument q; deserializeJson(q, json);
  JsonDocument rd; JsonObject r = rd.to<JsonObject>();
  ProtoCtx ctx{ PV_SERIAL, true };
  auto it = s_handlers.find(q["cmd"] | "");
  if (it != s_handlers.end()) it->second(q.as<JsonObjectConst>(), r, ctx);
}
// ---- fake IoT (real Notify/Desk call into it)
void iotBegin() {} void iotLoop() {} void iotEvent(const char*, const char*) {} void iotState(const char*, bool) {}
void iotButton(int, bool) {} const char* iotButtonLabel(int i) { static const char* L[] = {"Lights","Fan","Desk","TV","Scene","All off","Night","Heat","Music"}; return L[i]; }
IotMqttState iotMqttState() { return MQ_CONNECTED; } const char* iotMqttStateName() { return "Connected"; } bool iotMqttEnabled() { return true; }
int iotLastHookCode() { return 200; } const char* iotLastHookName() { return "Lights"; } const char* iotLastEvent() { return "button/up_short"; }
const char* iotDeviceId() { return "potahto_ddeeff"; }

// ---- fakes for modules not built into the simulator
void protoBegin() {} void protoSerialLoop() {}
void protoApplySideEffects(const char* k) { if (!strcmp(k, "rot180") || !strcmp(k, "autoRot")) { if (!S.autoRotate) uiSetRotation(S.rotate180); } }
void protoEmit(JsonObjectConst) {}
void bleLinkBegin() {} void bleLinkLoop() {} const char* blePeerName() { return fakeBle ? "MY-LAPTOP" : ""; }
void protoEmitBle(const char*) {}
int  netSaveNetwork(const char* ssid, const char*, const char*, uint8_t) { printf("[wifi] save %s\n", ssid); strlcpy(S.wifiSsid[0], ssid, 33); fnSlot = 0; fnState = NET_ONLINE; return 0; }
void netForget(int) {} void netConnectSlot(int s) { fnSlot = s; }
int  netCurrentSlot() { return fnState == NET_ONLINE ? fnSlot : -1; }
bool netEncEnterprise(uint8_t e) { return e == 5; } bool netEncOpen(uint8_t e) { return e == 0; }
uint16_t g_fakeTofMm = 900;
static bool s_slept = false;
void potahto_fake_deep_sleep() { s_slept = true; printf("[power] deep sleep\n"); throw 1; }
static const int PINS[5] = { PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_SELECT };
static int btnIdx(const std::string& s) {
  if (s == "up") return 0; if (s == "down") return 1; if (s == "left") return 2;
  if (s == "right") return 3; return 4;
}
// one frame of the REAL main loop (potahto.ino is compiled in)
static void frame() { g_fakeMillis += FRAME_MS; try { loop(); } catch (int) {} }
static void frames(int n) { for (int i = 0; i < n; i++) frame(); }
static void snap(const std::string& name) {
  uint8_t* b = u8g2.getBufferPtr();
  bool on = powerScreenOn() && !s_slept;
  std::ofstream f("shots/" + name + ".pbm");
  f << "P1\n128 64\n";
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) f << (on && ((b[(y / 8) * 128 + x] >> (y % 8)) & 1) ? "1 " : "0 "); f << "\n"; }
}
static void typeText(const std::string& t) {
  // drive the on-screen keyboard like a person: walk to each key, press SELECT
  static const char* P[4] = { "abcdefghijklmnopqrstuvwxyz.-_@", "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-_@", "1234567890!#$%&*()+=?/:;,'\"<>~", "[]{}\\|`^ .,-_@!?#$%&*()+=/:;'\"" };
  (void)P;
  extern void kbdTestType(const char*); kbdTestType(t.c_str());
}
int main(int argc, char** argv) {
  setup();
  strcpy(S.webPin, "482913");
  std::ifstream in(argv[1]); std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line); std::string c; ss >> c;
    if (c.empty() || c[0] == '#') continue;
    if (c == "frames") { int n; ss >> n; frames(n); }
    else if (c == "press") { std::string b; ss >> b; g_pinLevel[PINS[btnIdx(b)]] = 0; frames(3); g_pinLevel[PINS[btnIdx(b)]] = 1; frames(3); }
    else if (c == "hold") { std::string b; int ms; ss >> b >> ms; g_pinLevel[PINS[btnIdx(b)]] = 0; frames(ms / FRAME_MS + 1); g_pinLevel[PINS[btnIdx(b)]] = 1; frames(3); }
    else if (c == "down") { std::string b; ss >> b; g_pinLevel[PINS[btnIdx(b)]] = 0; frames(1); }
    else if (c == "up")   { std::string b; ss >> b; g_pinLevel[PINS[btnIdx(b)]] = 1; frames(1); }
    else if (c == "snap") { std::string n; ss >> n; snap(n); }
    else if (c == "net") { std::string s; ss >> s; fnState = s == "online" ? NET_ONLINE : s == "connecting" ? NET_CONNECTING : s == "noconfig" ? NET_NO_CONFIG : NET_OFF; }
    else if (c == "set") { std::string k, v; ss >> k; std::getline(ss, v); v.erase(0, v.find_first_not_of(' '));
      const SettingField* f = settingsFind(k.c_str()); if (f) settingsSetFromString(*f, v.c_str()); }
    else if (c == "adc") { ss >> g_fakeAdcMv; }
    else if (c == "tof") { ss >> g_fakeTofMm; }
    else if (c == "hid") { std::string v; ss >> v; fakeReady = v != "off"; if (v == "ble") { fakeBle = true; fakeRoute = HR_BLE; } if (v == "usb") fakeRoute = HR_USB; }
    else if (c == "time") { double e; ss >> e; timeSetEpoch(e, "USB"); }
    else if (c == "holdsnap") { std::string b, n; int ms; ss >> b >> ms >> n; g_pinLevel[PINS[btnIdx(b)]] = 0; frames(ms / FRAME_MS + 1); snap(n); g_pinLevel[PINS[btnIdx(b)]] = 1; frames(3); }
    else if (c == "proto") { std::string j; std::getline(ss, j); protoCall(j); }
    else if (c == "open") { int id; ss >> id; openApp(id); frames(2); }
    else if (c == "wait") { int ms; ss >> ms; frames(ms / FRAME_MS); }
    else if (c == "type") { std::string t; std::getline(ss, t); t.erase(0, t.find_first_not_of(' ')); typeText(t); }
    else if (c == "echo") { std::string t; std::getline(ss, t); printf("%s\n", t.c_str()); }
  }
  return 0;
}
