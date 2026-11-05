// Hunter, BLE scanner, LAN discovery, net doctor, Wake-on-LAN
#include "Multitool.h"
#include "Proto.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <NimBLEDevice.h>
#include "esp_http_client.h"
#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

//  Signal hunter
static bool     s_hunt = false, s_hScanning = false;
static uint8_t  s_hBssid[6], s_hCh = 0;
static char     s_hSsid[33] = "";
static int      s_hRssi = -127;
static uint32_t s_hSeen = 0;

void hunterStart(const uint8_t bssid[6], uint8_t ch, const char* ssid) {
  memcpy(s_hBssid, bssid, 6); s_hCh = ch; strlcpy(s_hSsid, ssid, sizeof(s_hSsid));
  s_hunt = true; s_hScanning = false; s_hRssi = -127; s_hSeen = millis();
  netExternalScan(true);
}
void hunterStop() {
  if (!s_hunt) return;
  s_hunt = false;
  if (s_hScanning) { WiFi.scanDelete(); s_hScanning = false; }
  netExternalScan(false);
}
bool hunterActive() { return s_hunt; }
int  hunterRssi()   { return s_hRssi; }
const char* hunterSsid() { return s_hSsid; }

static void hunterTick() {
  if (!s_hunt) return;
  uint32_t now = millis();
  const uint8_t* b = netBssid();
  if (b && !memcmp(b, s_hBssid, 6)) { s_hRssi = WiFi.RSSI(); s_hSeen = now; return; }   // our own AP: instant
  if (WiFi.getMode() == WIFI_OFF) return;
  if (!s_hScanning) {
    // single channel + BSSID filter: ~120 ms per sample
    if (WiFi.scanNetworks(true, true, false, 120, s_hCh, nullptr, s_hBssid) == WIFI_SCAN_RUNNING) s_hScanning = true;
    return;
  }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  for (int i = 0; i < n; i++) if (!memcmp(WiFi.BSSID(i), s_hBssid, 6)) { s_hRssi = WiFi.RSSI(i); s_hSeen = now; }
  WiFi.scanDelete(); s_hScanning = false;
  if (now - s_hSeen > 4000) s_hRssi = -127;
}

//  BLE scanner
static BleDev       s_ble[BLE_MAX];
static int          s_bleN = 0;
static portMUX_TYPE s_bleMux = portMUX_INITIALIZER_UNLOCKED;
static bool         s_bleOn = false;

struct Company { uint16_t id; const char* name; };
static const Company COMPANIES[] = {
  { 0x004C, "Apple" }, { 0x0006, "Microsoft" }, { 0x0075, "Samsung" }, { 0x00E0, "Google" },
  { 0x012D, "Sony" }, { 0x009E, "Bose" }, { 0x02E5, "Espressif" }, { 0x0059, "Nordic" },
  { 0x038F, "Xiaomi" }, { 0x027D, "Huawei" }, { 0x0087, "Garmin" },
};
const char* bleCompanyName(uint16_t id) {
  for (const Company& c : COMPANIES) if (c.id == id) return c.name;
  return nullptr;
}
const char* bleKindName(uint8_t k) {
  return k == BK_IBEACON ? "iBeacon" : k == BK_EDDYSTONE ? "Eddystone" : k == BK_FINDMY ? "Find My" : "";
}

class BleScanCb : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    BleDev x = {};
    memcpy(x.addr, d->getAddress().getVal(), 6);
    if (d->haveName()) strlcpy(x.name, d->getName().c_str(), sizeof(x.name));
    x.rssi = d->getRSSI(); x.rssiAvg = x.rssi; x.seen = millis();
    if (d->getManufacturerDataCount() > 0) {
      std::string md = d->getManufacturerData(0);
      if (md.size() >= 2) {
        x.company = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
        if (x.company == 0x004C && md.size() >= 4 && md[2] == 0x02 && md[3] == 0x15) x.kind = BK_IBEACON;
        else if (x.company == 0x004C && md.size() >= 3 && md[2] == 0x12) x.kind = BK_FINDMY;
      }
    }
    if (d->isAdvertisingService(NimBLEUUID((uint16_t)0xFEAA))) x.kind = BK_EDDYSTONE;
    portENTER_CRITICAL(&s_bleMux);
    int slot = -1, oldest = 0;
    for (int i = 0; i < s_bleN; i++) {
      if (!memcmp(s_ble[i].addr, x.addr, 6)) { slot = i; break; }
      if (s_ble[i].seen < s_ble[oldest].seen) oldest = i;
    }
    if (slot >= 0) {
      BleDev& e = s_ble[slot];
      e.rssi = x.rssi; e.rssiAvg = e.rssiAvg * 0.7f + x.rssi * 0.3f; e.seen = x.seen;
      if (x.name[0]) memcpy(e.name, x.name, sizeof(e.name));
      if (x.company) e.company = x.company;
      if (x.kind) e.kind = x.kind;
    } else if (s_bleN < BLE_MAX) s_ble[s_bleN++] = x;
    else s_ble[oldest] = x;
    portEXIT_CRITICAL(&s_bleMux);
  }
};
static BleScanCb s_bleCb;

void bleScanStart() {
  if (s_bleOn) return;
  if (!NimBLEDevice::isInitialized()) {
    if (S.bleMode != BM_OFF) hidBleStart(); else NimBLEDevice::init(S.devName);
  }
  NimBLEScan* sc = NimBLEDevice::getScan();
  sc->setScanCallbacks(&s_bleCb, true);          // duplicates on: live RSSI
  sc->setActiveScan(true);                       // ask for names (scan responses)
  sc->setInterval(100); sc->setWindow(90);
  sc->setMaxResults(0);                          // we keep our own table
  s_bleOn = sc->start(0, false, true);           // 0 = until stopped
}
void bleScanStop() {
  if (!s_bleOn) return;
  NimBLEDevice::getScan()->stop();
  s_bleOn = false;
}
bool bleScanning() { return s_bleOn; }
int bleSnapshot(BleDev* out, int max) {
  uint32_t now = millis();
  int n = 0;
  portENTER_CRITICAL(&s_bleMux);
  for (int i = 0; i < s_bleN && n < max; i++) if (now - s_ble[i].seen < 30000) out[n++] = s_ble[i];
  portEXIT_CRITICAL(&s_bleMux);
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && out[j].rssiAvg > out[j - 1].rssiAvg; j--) { BleDev t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
  return n;
}
bool bleLookup(const uint8_t addr[6], BleDev& out) {
  bool ok = false;
  portENTER_CRITICAL(&s_bleMux);
  for (int i = 0; i < s_bleN; i++) if (!memcmp(s_ble[i].addr, addr, 6)) { out = s_ble[i]; ok = true; break; }
  portEXIT_CRITICAL(&s_bleMux);
  return ok;
}

//  LAN scan (mDNS service discovery, background task)
struct Svc { const char* svc; const char* label; };
static const Svc SERVICES[] = {
  { "_workstation", "Computer" }, { "_googlecast", "Chromecast" }, { "_airplay", "AirPlay" },
  { "_ipp", "Printer" }, { "_printer", "Printer" }, { "_smb", "File share" }, { "_ssh", "SSH" },
  { "_hap", "HomeKit" }, { "_esphomelib", "ESPHome" }, { "_home-assistant", "Home Assistant" },
  { "_spotify-connect", "Spotify" }, { "_arduino", "Arduino" }, { "_http", "Web" },
};
static const int NSVC = sizeof(SERVICES) / sizeof(SERVICES[0]);
static LanHost      s_lan[LAN_MAX];
static volatile int s_lanN = 0, s_lanProg = 0;
static volatile bool s_lanBusy = false;
static portMUX_TYPE s_lanMux = portMUX_INITIALIZER_UNLOCKED;
const char* lanServiceName(uint8_t i) { return i < NSVC ? SERVICES[i].label : "?"; }

static void lanTask(void*) {
  for (int s = 0; s < NSVC && netOnline(); s++) {
    s_lanProg = s * 100 / NSVC;
    int n = MDNS.queryService(SERVICES[s].svc, "_tcp");
    for (int i = 0; i < n; i++) {
      LanHost h = {};
      strlcpy(h.host, MDNS.hostname(i).c_str(), sizeof(h.host));
      h.ip = (uint32_t)MDNS.address(i); h.port = MDNS.port(i); h.svc = (uint8_t)s;
      if (!h.ip) continue;
      portENTER_CRITICAL(&s_lanMux);
      bool dup = false;
      for (int k = 0; k < s_lanN; k++) if (s_lan[k].ip == h.ip) { dup = true; break; }
      if (!dup && s_lanN < LAN_MAX) { s_lan[s_lanN] = h; s_lanN = s_lanN + 1; }
      portEXIT_CRITICAL(&s_lanMux);
    }
  }
  s_lanProg = 100; s_lanBusy = false;
  vTaskDelete(nullptr);
}
void lanScanStart() {
  if (s_lanBusy || !netOnline()) return;
  s_lanN = 0; s_lanProg = 0; s_lanBusy = true;
  xTaskCreate(lanTask, "lanscan", 6144, nullptr, 1, nullptr);
}
bool lanScanning() { return s_lanBusy; }
int  lanProgress() { return s_lanProg; }
int  lanSnapshot(LanHost* out, int max) {
  portENTER_CRITICAL(&s_lanMux);
  int n = min((int)s_lanN, max);
  memcpy(out, s_lan, n * sizeof(LanHost));
  portEXIT_CRITICAL(&s_lanMux);
  return n;
}

//  Net doctor (background task)
static DocResult         s_doc[DS_COUNT];
static volatile bool     s_docBusy = false;
static SemaphoreHandle_t s_pingDone = nullptr;
static volatile uint32_t s_pingOk = 0, s_pingSum = 0;
const DocResult* doctorResults() { return s_doc; }
bool doctorRunning() { return s_docBusy; }
const char* doctorStepName(uint8_t i) {
  static const char* N[DS_COUNT] = { "WiFi link", "Router", "DNS", "Internet", "Latency" };
  return i < DS_COUNT ? N[i] : "";
}
static void pingOk(esp_ping_handle_t h, void*) {
  uint32_t t = 0; esp_ping_get_profile(h, ESP_PING_PROF_TIMEGAP, &t, sizeof(t));
  s_pingOk = s_pingOk + 1; s_pingSum = s_pingSum + t;
}
static void pingEnd(esp_ping_handle_t, void*) { xSemaphoreGive(s_pingDone); }
// returns replies (0..count), avg ms in *avg
static int pingHost(IPAddress ip, int count, int* avg) {
  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  IP_ADDR4(&cfg.target_addr, ip[0], ip[1], ip[2], ip[3]);
  cfg.count = count; cfg.interval_ms = 200; cfg.timeout_ms = 1000;
  esp_ping_callbacks_t cbs = {}; cbs.on_ping_success = pingOk; cbs.on_ping_end = pingEnd;
  esp_ping_handle_t h;
  s_pingOk = 0; s_pingSum = 0;
  if (esp_ping_new_session(&cfg, &cbs, &h) != ESP_OK) return -1;
  esp_ping_start(h);
  xSemaphoreTake(s_pingDone, pdMS_TO_TICKS(count * 1300 + 1000));
  esp_ping_stop(h); esp_ping_delete_session(h);
  *avg = s_pingOk ? (int)(s_pingSum / s_pingOk) : 0;
  return (int)s_pingOk;
}
static void setDoc(int i, int8_t st, int v, const char* note) { s_doc[i].value = v; strlcpy(s_doc[i].note, note, sizeof(s_doc[i].note)); s_doc[i].state = st; }
static void doctorTask(void*) {
  char n[24];
  if (!netOnline()) {
    setDoc(DS_LINK, 3, 0, "not connected");
    for (int i = 1; i < DS_COUNT; i++) setDoc(i, 3, 0, "skipped");
    s_docBusy = false; vTaskDelete(nullptr); return;
  }
  int r = WiFi.RSSI();
  snprintf(n, sizeof(n), "%d dBm, ch %d", r, WiFi.channel());
  setDoc(DS_LINK, r > -67 ? 1 : 2, r, n);
  int avg = 0, got = pingHost(WiFi.gatewayIP(), 4, &avg);
  if (got <= 0) setDoc(DS_GATEWAY, 3, 0, "no reply");
  else { snprintf(n, sizeof(n), "%d ms, %d%% loss", avg, (4 - got) * 25); setDoc(DS_GATEWAY, got == 4 && avg < 30 ? 1 : 2, avg, n); }
  uint32_t t0 = millis(); IPAddress ip;
  bool dnsOk = WiFi.hostByName("www.google.com", ip) == 1;
  uint32_t dt = millis() - t0;
  if (!dnsOk) setDoc(DS_DNS, 3, 0, "lookup failed");
  else { snprintf(n, sizeof(n), "%lu ms", (unsigned long)dt); setDoc(DS_DNS, dt < 300 ? 1 : 2, dt, n); }
  esp_http_client_config_t hc = {};
  hc.url = "http://connectivitycheck.gstatic.com/generate_204"; hc.timeout_ms = 5000;
  esp_http_client_handle_t c = esp_http_client_init(&hc);
  t0 = millis(); int code = -1;
  if (c) { if (esp_http_client_perform(c) == ESP_OK) code = esp_http_client_get_status_code(c); esp_http_client_cleanup(c); }
  dt = millis() - t0;
  if (code == 204) { snprintf(n, sizeof(n), "online, %lu ms", (unsigned long)dt); setDoc(DS_INTERNET, 1, dt, n); }
  else if (code > 0) { snprintf(n, sizeof(n), "HTTP %d: login page?", code); setDoc(DS_INTERNET, 2, code, n); }
  else setDoc(DS_INTERNET, 3, 0, "unreachable");
  got = pingHost(IPAddress(1, 1, 1, 1), 4, &avg);
  if (got <= 0) setDoc(DS_LATENCY, 3, 0, "no reply from 1.1.1.1");
  else { snprintf(n, sizeof(n), "%d ms, %d%% loss", avg, (4 - got) * 25); setDoc(DS_LATENCY, avg < 80 && got == 4 ? 1 : (avg < 200 ? 2 : 3), avg, n); }
  s_docBusy = false;
  vTaskDelete(nullptr);
}
void doctorRun() {
  if (s_docBusy) return;
  for (int i = 0; i < DS_COUNT; i++) setDoc(i, 0, 0, "");
  if (!s_pingDone) s_pingDone = xSemaphoreCreateBinary();
  s_docBusy = true;
  xTaskCreate(doctorTask, "netdoc", 6144, nullptr, 1, nullptr);
}

//  Wake-on-LAN
#define WOL_FILE "/wol.json"
struct WolDev { char name[20]; uint8_t mac[6]; };
static WolDev s_wol[WOL_MAX];
static int    s_wolN = 0;
static bool parseMac(const char* s, uint8_t m[6]) {
  unsigned v[6];
  if (sscanf(s, "%x%*[:-]%x%*[:-]%x%*[:-]%x%*[:-]%x%*[:-]%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) { if (v[i] > 255) return false; m[i] = (uint8_t)v[i]; }
  return true;
}
static void wolLoad() {
  s_wolN = 0;
  String s; JsonDocument d;
  if (!storeRead(WOL_FILE, s) || deserializeJson(d, s)) return;
  for (JsonObjectConst o : d["devices"].as<JsonArrayConst>()) {
    if (s_wolN >= WOL_MAX) break;
    if (!parseMac(o["mac"] | "", s_wol[s_wolN].mac)) continue;
    strlcpy(s_wol[s_wolN].name, o["name"] | "PC", sizeof(s_wol[0].name)); s_wolN++;
  }
}
int         wolCount()      { return s_wolN; }
const char* wolName(int i)  { return (i >= 0 && i < s_wolN) ? s_wol[i].name : ""; }
bool wolSend(int i) {
  if (i < 0 || i >= s_wolN || !netOnline()) return false;
  uint8_t pkt[102];
  memset(pkt, 0xFF, 6);
  for (int k = 0; k < 16; k++) memcpy(pkt + 6 + k * 6, s_wol[i].mac, 6);
  WiFiUDP u;
  bool ok = false;
  for (IPAddress dst : { WiFi.broadcastIP(), IPAddress(255, 255, 255, 255) }) {
    if (u.beginPacket(dst, 9)) { u.write(pkt, sizeof(pkt)); ok |= u.endPacket() == 1; }
  }
  return ok;
}

//  protocol
static void macStr(const uint8_t* a, char* o, bool reversed) {
  if (reversed) snprintf(o, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
  else          snprintf(o, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
}
static bool cBleScan(JsonObjectConst q, JsonObject r, ProtoCtx&) { if (q["on"] | true) bleScanStart(); else bleScanStop(); r["scanning"] = s_bleOn; return true; }
static bool cBleList(JsonObjectConst, JsonObject r, ProtoCtx&) {
  static BleDev tmp[BLE_MAX];
  int n = bleSnapshot(tmp, BLE_MAX);
  r["scanning"] = s_bleOn;
  JsonArray a = r["devices"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = a.add<JsonObject>(); char m[18]; macStr(tmp[i].addr, m, true);
    o["addr"] = m; o["name"] = tmp[i].name; o["rssi"] = (int)roundf(tmp[i].rssiAvg);
    const char* c = bleCompanyName(tmp[i].company); if (c) o["vendor"] = c; else if (tmp[i].company) o["vendorId"] = tmp[i].company;
    if (tmp[i].kind) o["kind"] = bleKindName(tmp[i].kind);
    o["age"] = (millis() - tmp[i].seen) / 1000;
  }
  return true;
}
static bool cLanScan(JsonObjectConst, JsonObject r, ProtoCtx&) {
  if (!netOnline()) { r["err"] = "WiFi not connected"; return false; }
  lanScanStart(); return true;
}
static bool cLanList(JsonObjectConst, JsonObject r, ProtoCtx&) {
  static LanHost tmp[LAN_MAX];
  int n = lanSnapshot(tmp, LAN_MAX);
  r["scanning"] = (bool)s_lanBusy; r["progress"] = (int)s_lanProg;
  JsonArray a = r["hosts"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = a.add<JsonObject>();
    o["host"] = tmp[i].host; o["ip"] = IPAddress(tmp[i].ip).toString(); o["port"] = tmp[i].port; o["type"] = lanServiceName(tmp[i].svc);
  }
  return true;
}
static bool cDoctorRun(JsonObjectConst, JsonObject, ProtoCtx&) { doctorRun(); return true; }
static bool cDoctorStatus(JsonObjectConst, JsonObject r, ProtoCtx&) {
  r["running"] = (bool)s_docBusy;
  static const char* ST[] = { "pending", "ok", "warn", "fail" };
  JsonArray a = r["steps"].to<JsonArray>();
  for (int i = 0; i < DS_COUNT; i++) { JsonObject o = a.add<JsonObject>(); o["name"] = doctorStepName(i); o["state"] = ST[s_doc[i].state & 3]; o["note"] = s_doc[i].note; }
  return true;
}
static bool cWolGet(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonArray a = r["devices"].to<JsonArray>();
  for (int i = 0; i < s_wolN; i++) { JsonObject o = a.add<JsonObject>(); char m[18]; macStr(s_wol[i].mac, m, false); o["name"] = s_wol[i].name; o["mac"] = m; }
  r["max"] = WOL_MAX;
  return true;
}
static bool cWolSet(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  JsonArrayConst a = q["devices"];
  if (a.size() > WOL_MAX) { r["err"] = "too many devices"; return false; }
  for (JsonObjectConst o : a) { uint8_t m[6]; if (!parseMac(o["mac"] | "", m)) { r["err"] = String("bad MAC for ") + (const char*)(o["name"] | "?"); return false; } }
  JsonDocument d; d["devices"] = a;
  String s; serializeJson(d, s);
  if (!storeWrite(WOL_FILE, s.c_str(), s.length())) { r["err"] = "could not save"; return false; }
  wolLoad(); return true;
}
static bool cWolSend(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  if (!netOnline()) { r["err"] = "WiFi not connected"; return false; }
  if (!wolSend(q["i"] | -1)) { r["err"] = "send failed"; return false; }
  return true;
}

void wirelessBegin() {
  wolLoad();
  protoRegister("ble.scan",      cBleScan);
  protoRegister("ble.list",      cBleList);
  protoRegister("lan.scan",      cLanScan);
  protoRegister("lan.list",      cLanList);
  protoRegister("doctor.run",    cDoctorRun);
  protoRegister("doctor.status", cDoctorStatus);
  protoRegister("wol.get",       cWolGet);
  protoRegister("wol.set",       cWolSet);
  protoRegister("wol.send",      cWolSend);
}
void wirelessLoop() { hunterTick(); }
