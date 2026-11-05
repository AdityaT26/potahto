// WiFi state machine, setup AP + captive DNS, mDNS, NTP, ArduinoOTA
#include "Multitool.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <time.h>
#if __has_include("esp_eap_client.h")
#include "esp_eap_client.h"
#endif

#define LINGER_MS          60000UL   // on-demand: keep radio this long after release
#define BOOT_ONLINE_MS     120000UL  // on-demand: online this long after power-up
#define CONNECT_TIMEOUT_MS 12000UL
#define RETRY_MS           30000UL

static NetState  s_state = NET_OFF;
static int       s_refs = 0;
static uint32_t  s_lingerUntil = 0;
static bool      s_lingering = false;
static bool      s_ap = false;
static DNSServer s_dns;
static char      s_host[24], s_apSsid[32], s_apPass[16], s_ip[16] = "";
static uint32_t  s_stateAt = 0, s_retryAt = 0;
static bool      s_services = false, s_webUp = false, s_otaInit = false;
static int8_t    s_order[WIFI_SLOTS];
static int       s_orderN = 0, s_orderIdx = 0;
static int8_t    s_prefer = -1;           // try this slot first (just joined from the device)
static int8_t    s_curSlot = -1;
static bool      s_ota = false;
static bool      s_userScan = false;
static bool      s_extScan = false;

static NetAP     s_aps[NET_MAX_APS];
static int       s_apsN = -2;          // -2 never scanned, -1 scanning

static void setState(NetState st) { s_state = st; s_stateAt = millis(); }

// ---- helpers
static void makeNames() {
  int j = 0;
  for (int i = 0; S.devName[i] && j < (int)sizeof(s_host) - 1; i++) {
    char c = S.devName[i];
    if (isalnum((unsigned char)c)) s_host[j++] = (char)tolower((unsigned char)c);
    else if (j > 0 && s_host[j - 1] != '-') s_host[j++] = '-';
  }
  while (j > 0 && s_host[j - 1] == '-') j--;
  s_host[j] = 0;
  if (!s_host[0]) strcpy(s_host, "potahto");
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(s_apSsid, sizeof(s_apSsid), "%.20s-%02X%02X", S.devName[0] ? S.devName : "Potahto", mac[4], mac[5]);
  snprintf(s_apPass, sizeof(s_apPass), "pt%s", S.webPin);          // 8 chars, WPA2 minimum
}

static bool anySaved() {
  for (int i = 0; i < WIFI_SLOTS; i++) if (S.wifiSsid[i][0]) return true;
  return false;
}

static bool wanted() {
  if (s_ap) return true;
  if (S.wifiMode == WM_OFF) return false;
  if (S.wifiMode == WM_ALWAYS) return true;
  if (s_refs > 0) return true;
  return s_lingering && (int32_t)(s_lingerUntil - millis()) > 0;
}

static void copyScan(int n) {
  s_apsN = 0;
  for (int i = 0; i < n && s_apsN < NET_MAX_APS; i++) {
    NetAP& a = s_aps[s_apsN++];
    strncpy(a.ssid, WiFi.SSID(i).c_str(), sizeof(a.ssid) - 1); a.ssid[sizeof(a.ssid) - 1] = 0;
    a.rssi = (int8_t)WiFi.RSSI(i);
    a.channel = (uint8_t)WiFi.channel(i);
    a.enc = (uint8_t)WiFi.encryptionType(i);
    memcpy(a.bssid, WiFi.BSSID(i), 6);
  }
  // strongest first
  for (int i = 1; i < s_apsN; i++)
    for (int j = i; j > 0 && s_aps[j].rssi > s_aps[j - 1].rssi; j--) { NetAP t = s_aps[j]; s_aps[j] = s_aps[j - 1]; s_aps[j - 1] = t; }
}

// ---- services (mDNS, NTP, ArduinoOTA, web)
static void otaSetup() {
  if (s_otaInit) return;
  ArduinoOTA.setHostname(s_host);
  ArduinoOTA.setPassword(S.webPin);
  ArduinoOTA.setMdnsEnabled(false);              // we run mDNS ourselves
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([]() {
    s_ota = true; motorOff(); buzzStop(); musicStop();
    otaDrawProgress("Arduino upload", 0, 1);
  });
  ArduinoOTA.onProgress([](unsigned int d, unsigned int t) { otaDrawProgress("Arduino upload", d, t); });
  ArduinoOTA.onEnd([]() { otaDrawProgress("Rebooting...", 1, 1); });
  ArduinoOTA.onError([](ota_error_t) { s_ota = false; toast("Upload failed"); fbError(); });
  ArduinoOTA.begin();
  s_otaInit = true;
}

static void servicesUp() {
  if (s_services) return;
  strncpy(s_ip, WiFi.localIP().toString().c_str(), sizeof(s_ip) - 1);
  if (MDNS.begin(s_host)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "device", "potahto");
    MDNS.enableArduino(3232, true);              // shows up as a Network port in the Arduino IDE
  }
  configTzTime(S.tz, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  otaSetup();
  s_services = true;
}
static void servicesDown() {
  if (!s_services) return;
  MDNS.end();
  s_services = false;
}

// ---- radio on/off
static void startScan() {
  WiFi.scanDelete();
  if (WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) { setState(NET_FAILED); s_retryAt = millis() + RETRY_MS; return; }
  s_apsN = -1;
  setState(NET_SCANNING);
}

static void radioOn() {
  makeNames();
  WiFi.persistent(false);                        // creds live in our settings, not the SDK
  WiFi.setHostname(s_host);
  WiFi.mode(s_ap ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(false);                  // the state machine handles it
  if (!s_webUp) { webStart(); s_webUp = true; }
  if (anySaved()) startScan();
  else            setState(NET_NO_CONFIG);
}

static void radioOff() {
  servicesDown();
  if (s_webUp) { webStop(); s_webUp = false; }
  if (s_otaInit) { ArduinoOTA.end(); s_otaInit = false; }
  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  s_ip[0] = 0;
  setState(NET_OFF);
}

static void tryNext() {
  while (s_orderIdx < s_orderN) {
    int slot = s_order[s_orderIdx++];
    if (!S.wifiSsid[slot][0]) continue;
    s_curSlot = (int8_t)slot;
#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
    if (S.wifiEap[slot] && S.wifiUser[slot][0]) {
      // campus / office WiFi (WPA2-Enterprise): username + password, no web page.
      // The server certificate is not checked (same as "Do not validate" on a
      // phone), which is what most campus setup guides ask for.
      WiFi.begin(S.wifiSsid[slot], S.wifiEap[slot] == 2 ? WPA2_AUTH_TTLS : WPA2_AUTH_PEAP,
                 S.wifiUser[slot], S.wifiUser[slot], S.wifiPass[slot]);
    } else {
      esp_wifi_sta_enterprise_disable();
      WiFi.begin(S.wifiSsid[slot], S.wifiPass[slot][0] ? S.wifiPass[slot] : nullptr);
    }
#else
    WiFi.begin(S.wifiSsid[slot], S.wifiPass[slot][0] ? S.wifiPass[slot] : nullptr);
#endif
    setState(NET_CONNECTING);
    return;
  }
  setState(NET_FAILED);
  s_retryAt = millis() + RETRY_MS;
}

// ---- public
void netBegin() {
  makeNames();
  WiFi.mode(WIFI_OFF);
  setState(NET_OFF);
  // "On demand": still go online at power-up (clock sync, MQTT check-in, web
  // app reachable for a bit), then nap until an app needs the radio. Not on an
  // alarm wake from deep sleep: that one only rings.
  if (S.wifiMode == WM_ON_DEMAND && anySaved() && strcmp(powerWakeReason(), "alarm") != 0) {
    s_lingering = true; s_lingerUntil = millis() + BOOT_ONLINE_MS;
  }
}

void netAcquire() { s_refs++; }
void netRelease() {
  if (s_refs > 0 && --s_refs == 0) { s_lingering = true; s_lingerUntil = millis() + LINGER_MS; }
}

void netReconnect() {
  if (s_state == NET_OFF) { s_lingering = true; s_lingerUntil = millis() + LINGER_MS; return; }   // start now, try it
  servicesDown();
  WiFi.disconnect(false);
  makeNames();
  if (s_ap) { WiFi.softAPsetHostname(s_host); }
  if (anySaved()) startScan(); else setState(NET_NO_CONFIG);
}

void netSetupAP(bool on) {
  if (on == s_ap) return;
  s_ap = on;
  if (on) {
    makeNames();
    if (s_state == NET_OFF) radioOn();
    else WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(s_apSsid, s_apPass);
    delay(100);
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", WiFi.softAPIP());       // captive portal: every name -> us
  } else {
    s_dns.stop();
    WiFi.softAPdisconnect(true);
    if (s_state != NET_OFF) WiFi.mode(WIFI_STA);
  }
}
bool netSetupAPActive() { return s_ap; }

void netExternalScan(bool on) { s_extScan = on; }
bool netEncOpen(uint8_t e) { return e == WIFI_AUTH_OPEN; }
// Newer ESP-IDF versions (core 3.1+) added more enterprise auth modes. Look
// them up through the enum type so older cores (3.0.x) still compile: a name
// the core doesn't have resolves to -1 instead of a compile error.
#define AUTH_IF_ANY(NAME)                                                                   \
  template <typename E, typename = void> struct Has_##NAME { static constexpr int v = -1; }; \
  template <typename E> struct Has_##NAME<E, decltype((void)E::NAME)> { static constexpr int v = (int)E::NAME; };
AUTH_IF_ANY(WIFI_AUTH_WPA3_ENTERPRISE)
AUTH_IF_ANY(WIFI_AUTH_WPA2_WPA3_ENTERPRISE)
AUTH_IF_ANY(WIFI_AUTH_WPA_ENTERPRISE)
AUTH_IF_ANY(WIFI_AUTH_WPA3_ENT_192)
#undef AUTH_IF_ANY
bool netEncEnterprise(uint8_t e) {
  static const int EXTRA[] = { Has_WIFI_AUTH_WPA3_ENTERPRISE<wifi_auth_mode_t>::v, Has_WIFI_AUTH_WPA2_WPA3_ENTERPRISE<wifi_auth_mode_t>::v,
                               Has_WIFI_AUTH_WPA_ENTERPRISE<wifi_auth_mode_t>::v, Has_WIFI_AUTH_WPA3_ENT_192<wifi_auth_mode_t>::v };
  if (e == WIFI_AUTH_ENTERPRISE) return true;                    // = WPA2-Enterprise, on every core
  for (int x : EXTRA) if (x >= 0 && e == x) return true;
  return false;
}
int netCurrentSlot() { return s_state == NET_ONLINE ? s_curSlot : -1; }
int netSaveNetwork(const char* ssid, const char* pass, const char* user, uint8_t eap) {
  if (!ssid || !ssid[0] || strlen(ssid) > 32) return -1;
  int slot = -1;
  for (int i = 0; i < WIFI_SLOTS; i++) if (!strcmp(S.wifiSsid[i], ssid)) { slot = i; break; }
  if (slot < 0) {                              // newest first: shift the others down
    for (int i = WIFI_SLOTS - 1; i > 0; i--) {
      strcpy(S.wifiSsid[i], S.wifiSsid[i - 1]); strcpy(S.wifiPass[i], S.wifiPass[i - 1]);
      strcpy(S.wifiUser[i], S.wifiUser[i - 1]); S.wifiEap[i] = S.wifiEap[i - 1];
    }
    slot = 0;
  }
  strlcpy(S.wifiSsid[slot], ssid, sizeof(S.wifiSsid[0]));
  strlcpy(S.wifiPass[slot], pass ? pass : "", sizeof(S.wifiPass[0]));
  strlcpy(S.wifiUser[slot], user ? user : "", sizeof(S.wifiUser[0]));
  S.wifiEap[slot] = (user && user[0]) ? (eap ? eap : 1) : 0;
  settingsSave();
  s_prefer = (int8_t)slot;
  netReconnect();
  return slot;
}
void netForget(int slot) {
  if (slot < 0 || slot >= WIFI_SLOTS) return;
  for (int i = slot; i < WIFI_SLOTS - 1; i++) {
    strcpy(S.wifiSsid[i], S.wifiSsid[i + 1]); strcpy(S.wifiPass[i], S.wifiPass[i + 1]);
    strcpy(S.wifiUser[i], S.wifiUser[i + 1]); S.wifiEap[i] = S.wifiEap[i + 1];
  }
  S.wifiSsid[WIFI_SLOTS - 1][0] = 0; S.wifiPass[WIFI_SLOTS - 1][0] = 0; S.wifiUser[WIFI_SLOTS - 1][0] = 0; S.wifiEap[WIFI_SLOTS - 1] = 0;
  settingsSave();
  netReconnect();
}
void netConnectSlot(int slot) { if (slot >= 0 && slot < WIFI_SLOTS) { s_prefer = (int8_t)slot; netReconnect(); } }
const uint8_t* netBssid() { return s_state == NET_ONLINE ? WiFi.BSSID() : nullptr; }
int netChannel() { return s_state == NET_ONLINE ? WiFi.channel() : 0; }
void netScanStart() {
  if (s_extScan) return;
  if (s_state == NET_OFF) { s_lingering = true; s_lingerUntil = millis() + LINGER_MS; return; }  // radio scan follows
  if (s_state == NET_SCANNING || s_userScan) return;
  WiFi.scanDelete();
  if (WiFi.scanNetworks(true, false) != WIFI_SCAN_FAILED) { s_userScan = true; s_apsN = -1; }
}
const NetAP* netScanResults(int* n) { if (n) *n = s_apsN; return s_aps; }

void netLoop() {
  if (s_ap) s_dns.processNextRequest();
  if (s_webUp) webLoop();
  if (s_otaInit) ArduinoOTA.handle();

  static uint32_t s_tick = 0;
  uint32_t now = millis();
  if (now - s_tick < 100) return;                 // state machine at 10 Hz
  s_tick = now;

  if (s_lingering && s_refs == 0 && (int32_t)(s_lingerUntil - now) <= 0) s_lingering = false;
  bool want = wanted();
  if (!want) { if (s_state != NET_OFF) radioOff(); return; }
  if (s_state == NET_OFF) { radioOn(); return; }

  if (s_extScan) {                                // a tool owns the scanner right now
    if (s_state == NET_ONLINE && WiFi.status() != WL_CONNECTED) { servicesDown(); s_ip[0] = 0; setState(NET_FAILED); s_retryAt = now + 2000; }
    return;
  }
  // user-requested scan while connected
  if (s_userScan) {
    int n = WiFi.scanComplete();
    if (n >= 0) { copyScan(n); WiFi.scanDelete(); s_userScan = false; }
    else if (n == WIFI_SCAN_FAILED) { s_apsN = 0; s_userScan = false; }
  }

  switch (s_state) {
    case NET_SCANNING: {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) break;
      if (n < 0) { setState(NET_FAILED); s_retryAt = now + RETRY_MS; break; }
      copyScan(n);
      WiFi.scanDelete();
      // order saved networks that are in range by signal strength
      s_orderN = 0; s_orderIdx = 0;
      for (int a = 0; a < s_apsN && s_orderN < WIFI_SLOTS; a++)
        for (int slot = 0; slot < WIFI_SLOTS; slot++)
          if (S.wifiSsid[slot][0] && !strcmp(S.wifiSsid[slot], s_aps[a].ssid)) {
            bool dup = false;
            for (int k = 0; k < s_orderN; k++) if (s_order[k] == slot) dup = true;
            if (!dup) s_order[s_orderN++] = (int8_t)slot;
          }
      // a network just joined from the device goes first
      if (s_prefer >= 0 && S.wifiSsid[s_prefer][0]) {
        for (int k = 0; k < s_orderN; k++) if (s_order[k] == s_prefer) { memmove(s_order + 1, s_order, k); s_order[0] = s_prefer; break; }
        bool have = false; for (int k = 0; k < s_orderN; k++) if (s_order[k] == s_prefer) have = true;
        if (!have && s_orderN < WIFI_SLOTS) { memmove(s_order + 1, s_order, s_orderN); s_order[0] = s_prefer; s_orderN++; }
        s_prefer = -1;
      }
      // hidden networks never show in a scan: try the rest last
      for (int slot = 0; slot < WIFI_SLOTS && s_orderN < WIFI_SLOTS; slot++) {
        bool dup = false;
        for (int k = 0; k < s_orderN; k++) if (s_order[k] == slot) dup = true;
        if (!dup && S.wifiSsid[slot][0]) s_order[s_orderN++] = (int8_t)slot;
      }
      tryNext();
    } break;
    case NET_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        setState(NET_ONLINE); servicesUp();
        WiFi.setSleep(true);                      // modem sleep between beacons: big battery saver
      }
      else if (now - s_stateAt > CONNECT_TIMEOUT_MS) { WiFi.disconnect(false); tryNext(); }
      break;
    case NET_ONLINE:
      if (WiFi.status() != WL_CONNECTED) { servicesDown(); s_ip[0] = 0; startScan(); }
      break;
    case NET_FAILED:
      if ((int32_t)(now - s_retryAt) >= 0) { if (anySaved()) startScan(); else setState(NET_NO_CONFIG); }
      break;
    case NET_NO_CONFIG:
      if (anySaved()) startScan();
      break;
    default: break;
  }
}

NetState    netState()   { return s_state; }
bool        netOnline()  { return s_state == NET_ONLINE; }
const char* netStateName() {
  switch (s_state) {
    case NET_OFF:        return s_ap ? "Setup AP" : "Off";
    case NET_SCANNING:   return "Scanning";
    case NET_CONNECTING: return "Connecting";
    case NET_ONLINE:     return "Online";
    case NET_NO_CONFIG:  return "No network saved";
    case NET_FAILED:     return "Not found";
  }
  return "?";
}
const char* netSsid() {
  static char ss[33];
  if (s_state != NET_ONLINE) return "";
  strncpy(ss, WiFi.SSID().c_str(), sizeof(ss) - 1); ss[sizeof(ss) - 1] = 0;
  return ss;
}
const char* netIp() {
  static char apip[16];
  if (s_state == NET_ONLINE) return s_ip;
  if (s_ap) { strncpy(apip, WiFi.softAPIP().toString().c_str(), sizeof(apip) - 1); apip[sizeof(apip) - 1] = 0; return apip; }
  return "";
}
int         netRssi()     { return s_state == NET_ONLINE ? WiFi.RSSI() : 0; }
const char* netHostname() { return s_host; }
const char* netApSsid()   { return s_apSsid; }
const char* netApPass()   { return s_apPass; }
bool        netTimeValid(){ return timeValid(); }

// ---- OTA progress view
bool otaActive() { return s_ota; }
void otaSetActive(bool on) { s_ota = on; if (on) { motorOff(); buzzStop(); musicStop(); } }
void otaDrawProgress(const char* title, uint32_t done, uint32_t total) {
  u8g2.setDisplayRotation(U8G2_R0);
  u8g2.clearBuffer();
  u8g2.setFont(F_UI);
  uiCenter(14, "Firmware update");
  u8g2.setFont(F_SMALL);
  uiCenter(28, title);
  float f = total ? (float)done / total : 0;
  uiProgressBar(10, 36, 108, 10, f);
  char p[24];
  if (total > 1) snprintf(p, sizeof(p), "%u%%  %lu KB", (unsigned)(f * 100), (unsigned long)(done / 1024));
  else           snprintf(p, sizeof(p), "%lu KB", (unsigned long)(done / 1024));
  uiCenter(58, p);
  u8g2.sendBuffer();
  npxSolid(strip.Color(0, 60, 160)); npxForceRefresh();
}
