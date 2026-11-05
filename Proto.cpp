// JSON config protocol: registry, core commands, serial transport
#include "Multitool.h"
#include "Proto.h"
#include <WiFi.h>
#include <time.h>

#define PROTO_MAX_CMDS   128
#define SERIAL_LINE_MAX  8192
#define PIN_MAX_FAILS    5
#define PIN_LOCK_MS      60000UL

struct Cmd { const char* name; ProtoFn fn; bool auth; };
static Cmd    s_cmds[PROTO_MAX_CMDS];
static int    s_nCmds = 0;
static char*  s_line = nullptr;
static size_t s_lineLen = 0;
static bool   s_lineOverflow = false;
static uint8_t  s_pinFails = 0;
static uint32_t s_pinLockUntil = 0;

void protoRegister(const char* cmd, ProtoFn fn, bool needAuth) {
  for (int i = 0; i < s_nCmds; i++) if (!strcmp(s_cmds[i].name, cmd)) { s_cmds[i].fn = fn; s_cmds[i].auth = needAuth; return; }
  if (s_nCmds < PROTO_MAX_CMDS) s_cmds[s_nCmds++] = { cmd, fn, needAuth };
}

bool protoPinCheck(const char* pin) {
  uint32_t now = millis();
  if (s_pinFails >= PIN_MAX_FAILS) {
    if ((int32_t)(s_pinLockUntil - now) > 0) return false;
    s_pinFails = 0;
  }
  // constant-time compare
  const char* want = S.webPin;
  size_t lw = strlen(want), lp = pin ? strlen(pin) : 0;
  uint8_t diff = (uint8_t)(lw ^ lp);
  for (size_t i = 0; i < lw; i++) diff |= (uint8_t)(want[i] ^ (i < lp ? pin[i] : 0));
  if (diff == 0 && lw > 0) { s_pinFails = 0; return true; }
  if (++s_pinFails >= PIN_MAX_FAILS) s_pinLockUntil = now + PIN_LOCK_MS;
  return false;
}

// ---- field <-> JSON
static void hexOf(const uint8_t* p, size_t n, char* out) {
  static const char* H = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) { out[2 * i] = H[p[i] >> 4]; out[2 * i + 1] = H[p[i] & 15]; }
  out[2 * n] = 0;
}
static bool unhex(const char* s, uint8_t* p, size_t n) {
  if (!s || strlen(s) != 2 * n) return false;
  for (size_t i = 0; i < n; i++) {
    int hi = isdigit((unsigned char)s[2*i]) ? s[2*i] - '0' : (tolower((unsigned char)s[2*i]) - 'a' + 10);
    int lo = isdigit((unsigned char)s[2*i+1]) ? s[2*i+1] - '0' : (tolower((unsigned char)s[2*i+1]) - 'a' + 10);
    if (hi < 0 || hi > 15 || lo < 0 || lo > 15) return false;
    p[i] = (uint8_t)(hi << 4 | lo);
  }
  return true;
}

void protoFieldValue(JsonObject dst, const SettingField& f, bool withSecrets) {
  if ((f.flags & SF_SECRET) && !withSecrets) return;
  switch (f.type) {
    case ST_BOOL: dst[f.key] = *(bool*)f.ptr; break;
    case ST_U8: case ST_U16: case ST_I16: dst[f.key] = (long)settingsGetAsDouble(f); break;
    case ST_F32:  dst[f.key] = serialized(String(*(float*)f.ptr, 4)); break;
    case ST_STR:  dst[f.key] = (const char*)f.ptr; break;
    case ST_BLOB: { char h[2 * 32 + 1]; if (f.size <= 32) { hexOf((const uint8_t*)f.ptr, f.size, h); dst[f.key] = h; } } break;
  }
}

static bool setField(const SettingField& f, JsonVariantConst v) {
  if (f.type == ST_BLOB) {
    uint8_t tmp[32];
    if (f.size > sizeof(tmp) || !v.is<const char*>() || !unhex(v.as<const char*>(), tmp, f.size)) return false;
    memcpy(f.ptr, tmp, f.size); return true;
  }
  if (v.is<bool>())        return settingsSetFromDouble(f, v.as<bool>() ? 1 : 0);
  if (v.is<const char*>()) return settingsSetFromString(f, v.as<const char*>());
  if (v.is<double>() || v.is<long>()) return settingsSetFromDouble(f, v.as<double>());
  return false;
}

void protoApplySideEffects(const char* k) {
  if (!strcmp(k, "rot180") || !strcmp(k, "autoRot")) { if (!S.autoRotate) uiSetRotation(S.rotate180); }
  else if (!strcmp(k, "npxEn") || !strcmp(k, "npxBri")) npxForceRefresh();
  else if (!strcmp(k, "motEn")) { if (!S.motorEnabled) motorOff(); }
  else if (!strcmp(k, "buzEn")) { if (!S.buzzerEnabled) buzzStop(); }
  else if (!strcmp(k, "tz")) timeApplyTz();
  else if (!strcmp(k, "bleMode")) { if (S.bleMode != BM_OFF) hidBleStart(); }
  else if (!strncmp(k, "wSsid", 5) || !strncmp(k, "wPass", 5) || !strncmp(k, "wUser", 5) || !strncmp(k, "wEap", 4) || !strcmp(k, "wifiMode")) netReconnect();
}

// ---- core commands
static bool cHello(JsonObjectConst, JsonObject r, ProtoCtx& c) {
  r["device"] = "potahto";
  r["name"] = S.devName;
  r["fw"] = FW_VERSION;
  r["proto"] = 1;
  r["model"] = "XIAO ESP32-S3";
  r["host"] = netHostname();
  r["mac"] = WiFi.macAddress();
  r["needPin"] = (c.via == PV_HTTP) || !c.authed;
  r["via"] = c.via == PV_SERIAL ? "usb" : c.via == PV_BLE ? "ble" : "wifi";
  r["authed"] = c.authed;
  return true;
}
static bool cAuth(JsonObjectConst q, JsonObject r, ProtoCtx& c) {
  if (c.via == PV_SERIAL || c.authed) { c.authed = true; return true; }
  if ((int32_t)(s_pinLockUntil - millis()) > 0 && s_pinFails >= PIN_MAX_FAILS) { r["err"] = "locked"; r["retryIn"] = (s_pinLockUntil - millis()) / 1000; return false; }
  if (!protoPinCheck(q["pin"] | "")) { r["err"] = "bad pin"; return false; }
  c.authed = true;
  return true;
}
static bool cStatus(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonObject b = r["battery"].to<JsonObject>();
  b["pct"] = battPercent();
  b["v"] = serialized(String(battVoltage(), 2));
  b["charging"] = battCharging();
  JsonObject w = r["wifi"].to<JsonObject>();
  w["state"] = netStateName();
  w["ssid"] = netSsid();
  w["ip"] = netIp();
  w["rssi"] = netRssi();
  w["host"] = netHostname();
  w["ap"] = netSetupAPActive();
  w["mode"] = S.wifiMode;
  r["ble"] = g_bleConnected;
  r["bleHost"] = blePeerName();
  r["route"] = hidRouteName();
  r["screen"] = powerScreenOn();
  r["timeSrc"] = timeSource();
  r["trip"] = tripArmed(); r["guard"] = guardArmed();
  r["app"] = APPS[currentApp()].name;
  r["uptime"] = millis() / 1000;
  r["heap"] = ESP.getFreeHeap();
  r["psram"] = ESP.getPsramSize();
  r["sensors"]["mpu"] = mpuPresent();
  r["sensors"]["compass"] = compassPresent();
  r["sensors"]["tof"] = tofPresent();
  if (timeValid()) {
    time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
    char ts[24]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &lt); r["time"] = ts;
  }
  return true;
}
static bool cSchema(JsonObjectConst, JsonObject r, ProtoCtx& c) {
  size_t n; const SettingField* F = settingsFields(&n);
  JsonArray a = r["fields"].to<JsonArray>();
  static const char* TN[] = { "bool", "u8", "u16", "i16", "f32", "blob", "str" };
  for (size_t i = 0; i < n; i++) {
    const SettingField& f = F[i];
    if (f.flags & SF_HIDDEN) continue;
    JsonObject o = a.add<JsonObject>();
    o["k"] = f.key; o["t"] = TN[f.type]; o["g"] = f.group; o["l"] = f.label;
    if (f.type != ST_BOOL && f.type != ST_STR && f.type != ST_BLOB) { o["lo"] = f.lo; o["hi"] = f.hi; o["st"] = f.step; }
    if (f.type == ST_STR) o["max"] = f.size - 1;
    if (f.opts) o["o"] = f.opts;
    if (f.flags) o["f"] = f.flags;
    if (f.flags & SF_SECRET) {
      bool showSecret = (c.via == PV_SERIAL) && strcmp(f.key, "webPin") == 0;   // USB may read the PIN
      o["set"] = ((const char*)f.ptr)[0] != 0;
      if (showSecret) o["v"] = (const char*)f.ptr;
    } else {
      JsonObject tmp = o; // write value under "v"
      switch (f.type) {
        case ST_BOOL: tmp["v"] = *(bool*)f.ptr; break;
        case ST_F32:  tmp["v"] = serialized(String(*(float*)f.ptr, 4)); break;
        case ST_STR:  tmp["v"] = (const char*)f.ptr; break;
        default:      tmp["v"] = (long)settingsGetAsDouble(f); break;
      }
    }
  }
  return true;
}
static bool cGet(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  JsonObject v = r["values"].to<JsonObject>();
  JsonArrayConst keys = q["keys"];
  size_t n; const SettingField* F = settingsFields(&n);
  for (size_t i = 0; i < n; i++) {
    if (F[i].flags & SF_HIDDEN) continue;
    if (!keys.isNull()) { bool want = false; for (JsonVariantConst k : keys) if (k.is<const char*>() && !strcmp(k.as<const char*>(), F[i].key)) want = true; if (!want) continue; }
    protoFieldValue(v, F[i], false);
  }
  return true;
}
static bool cSet(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  JsonObjectConst vals = q["values"];
  if (vals.isNull()) { r["err"] = "missing values"; return false; }
  JsonArray applied = r["applied"].to<JsonArray>();
  JsonObject errs = r["errors"].to<JsonObject>();
  bool reboot = false;
  for (JsonPairConst kv : vals) {
    const SettingField* f = settingsFind(kv.key().c_str());
    if (!f || (f->flags & SF_HIDDEN)) { errs[kv.key()] = "unknown"; continue; }
    if (!setField(*f, kv.value())) { errs[kv.key()] = "invalid"; continue; }
    applied.add(f->key);
    if (f->flags & SF_REBOOT) reboot = true;
    protoApplySideEffects(f->key);
  }
  if (applied.size()) settingsSaveSoon();
  r["reboot"] = reboot;
  return errs.size() == 0 || applied.size() > 0;
}
static bool cSave(JsonObjectConst, JsonObject, ProtoCtx&) { settingsSave(); return true; }
static uint32_t s_rebootAt = 0;
static bool cReboot(JsonObjectConst, JsonObject, ProtoCtx&) { settingsSave(); s_rebootAt = millis() + 600; return true; }
static uint32_t s_sleepAt = 0;
// {"cmd":"power","do":"off"|"lock"|"wake"|"sleep"}
static bool cPower(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  const char* d = q["do"] | "";
  if      (!strcmp(d, "off"))   powerScreenOff(false);
  else if (!strcmp(d, "lock"))  { if (!S.lockMode) { r["err"] = "lock screen is off in Settings"; return false; } powerLockNow(); }
  else if (!strcmp(d, "wake"))  powerWake();
  else if (!strcmp(d, "sleep")) { settingsSave(); s_sleepAt = millis() + 600; }
  else { r["err"] = "do: off / lock / wake / sleep"; return false; }
  return true;
}
static bool cFactory(JsonObjectConst, JsonObject, ProtoCtx&) {
  settingsFactoryReset(); uiSetRotation(S.rotate180); netReconnect(); toast("Reset done"); return true;
}
static bool cBackup(JsonObjectConst q, JsonObject r, ProtoCtx& c) {
  bool secrets = (c.via == PV_SERIAL) && (q["secrets"] | false);
  r["potahtoBackup"] = 1; r["fw"] = FW_VERSION; r["name"] = S.devName;
  JsonObject v = r["settings"].to<JsonObject>();
  size_t n; const SettingField* F = settingsFields(&n);
  for (size_t i = 0; i < n; i++) protoFieldValue(v, F[i], secrets);   // includes calibration
  JsonObject hs = r["hiscores"].to<JsonObject>();
  for (int i = 0; i < gamesCount(); i++) hs[gameHsKey(i)] = hiscoreGet(gameHsKey(i));
  return true;
}
static bool cRestore(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  JsonObjectConst v = q["settings"];
  if (v.isNull()) { r["err"] = "not a backup"; return false; }
  int ok = 0, bad = 0;
  for (JsonPairConst kv : v) {
    const SettingField* f = settingsFind(kv.key().c_str());
    if (f && setField(*f, kv.value())) { ok++; protoApplySideEffects(f->key); } else bad++;
  }
  JsonObjectConst hs = q["hiscores"];
  if (!hs.isNull()) for (JsonPairConst kv : hs) {
    for (int i = 0; i < gamesCount(); i++) if (!strcmp(gameHsKey(i), kv.key().c_str()) && kv.value().is<long>()) hiscoreSet(gameHsKey(i), kv.value().as<long>());
  }
  settingsSave();
  r["restored"] = ok; r["skipped"] = bad;
  return true;
}
static bool cWifiScan(JsonObjectConst, JsonObject, ProtoCtx&) { netScanStart(); return true; }
static bool cWifiList(JsonObjectConst, JsonObject r, ProtoCtx&) {
  int n; const NetAP* a = netScanResults(&n);
  r["state"] = n;                                        // -2 none, -1 running
  JsonArray arr = r["aps"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    if (!a[i].ssid[0]) continue;
    bool dup = false; for (int j = 0; j < i; j++) if (!strcmp(a[j].ssid, a[i].ssid)) dup = true;
    if (dup) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = a[i].ssid; o["rssi"] = a[i].rssi; o["ch"] = a[i].channel; o["open"] = netEncOpen(a[i].enc); o["ent"] = netEncEnterprise(a[i].enc);
  }
  return true;
}
static bool cWifiSetup(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  netSetupAP(q["on"] | false);
  r["ap"] = netSetupAPActive(); r["ssid"] = netApSsid();
  return true;
}
static bool cHiscores(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  if (q["reset"] | false) for (int i = 0; i < gamesCount(); i++) hiscoreSet(gameHsKey(i), 0);
  JsonArray a = r["games"].to<JsonArray>();
  for (int i = 0; i < gamesCount(); i++) {
    JsonObject o = a.add<JsonObject>();
    o["name"] = gameName(i); o["key"] = gameHsKey(i); o["best"] = hiscoreGet(gameHsKey(i));
  }
  return true;
}
static bool cToast(JsonObjectConst q, JsonObject, ProtoCtx&) {
  toast(q["text"] | "Hello!"); fbToastCue(); return true;
}
static bool cCalGyro(JsonObjectConst, JsonObject r, ProtoCtx&) {
  if (!mpuPresent()) { r["err"] = "no MPU"; return false; }
  mpuCalibrateGyro(); toast("Gyro zeroed"); return true;
}
static bool cCalLevel(JsonObjectConst, JsonObject r, ProtoCtx&) {
  if (!mpuPresent()) { r["err"] = "no MPU"; return false; }
  mpuLevelHere(); toast("Leveled"); return true;
}
static bool cCommands(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonArray a = r["commands"].to<JsonArray>();
  for (int i = 0; i < s_nCmds; i++) a.add(s_cmds[i].name);
  return true;
}

// ---- events
#define EVQ 8
static String s_evq[EVQ]; static uint8_t s_evN = 0;
void protoEmit(JsonObjectConst ev) {
  String line; serializeJson(ev, line);
  if (Serial) Serial.println(line);
  protoEmitBle(line.c_str());
  if (s_evN < EVQ) s_evq[s_evN++] = line; else { for (int i = 1; i < EVQ; i++) s_evq[i - 1] = s_evq[i]; s_evq[EVQ - 1] = line; }
}
static bool cEvents(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonArray a = r["events"].to<JsonArray>();
  for (int i = 0; i < s_evN; i++) { JsonDocument d; if (!deserializeJson(d, s_evq[i])) a.add(d.as<JsonObjectConst>()); s_evq[i] = String(); }
  s_evN = 0;
  return true;
}

void protoBegin() {
  protoRegister("events",   cEvents);
  protoRegister("hello",    cHello,    false);
  protoRegister("auth",     cAuth,     false);
  protoRegister("commands", cCommands);
  protoRegister("status",   cStatus);
  protoRegister("schema",   cSchema);
  protoRegister("get",      cGet);
  protoRegister("set",      cSet);
  protoRegister("save",     cSave);
  protoRegister("reboot",   cReboot);
  protoRegister("power",    cPower);
  protoRegister("factory",  cFactory);
  protoRegister("backup",   cBackup);
  protoRegister("restore",  cRestore);
  protoRegister("wifi.scan",  cWifiScan);
  protoRegister("wifi.list",  cWifiList);
  protoRegister("wifi.setup", cWifiSetup);
  protoRegister("hiscores", cHiscores);
  protoRegister("toast",    cToast);
  protoRegister("cal.gyro", cCalGyro);
  protoRegister("cal.level",cCalLevel);
  s_line = (char*)malloc(SERIAL_LINE_MAX);
}

// ---- dispatch
void protoHandle(const char* body, size_t len, String& out, ProtoCtx& ctx) {
  JsonDocument req;
  JsonDocument resp;
  JsonObject r = resp.to<JsonObject>();
  DeserializationError e = deserializeJson(req, body, len);
  if (e || !req.is<JsonObject>()) {
    r["ok"] = false; r["err"] = "bad json";
    serializeJson(resp, out); return;
  }
  JsonObjectConst q = req.as<JsonObjectConst>();
  if (!q["id"].isNull()) r["id"] = q["id"];
  const char* cmd = q["cmd"] | "";
  const Cmd* c = nullptr;
  for (int i = 0; i < s_nCmds; i++) if (!strcmp(s_cmds[i].name, cmd)) { c = &s_cmds[i]; break; }
  if (!c) { r["ok"] = false; r["err"] = "unknown cmd"; serializeJson(resp, out); return; }
  if (c->auth && !ctx.authed) { r["ok"] = false; r["err"] = "auth"; serializeJson(resp, out); return; }
  bool ok = c->fn(q, r, ctx);
  r["ok"] = ok;
  if (!ok && r["err"].isNull()) r["err"] = "failed";
  serializeJson(resp, out);
}

// ---- serial transport
void protoSerialLoop() {
  if (s_rebootAt && (int32_t)(millis() - s_rebootAt) >= 0) { Serial.flush(); ESP.restart(); }
  if (s_sleepAt && (int32_t)(millis() - s_sleepAt) >= 0) { s_sleepAt = 0; Serial.flush(); powerDeepSleep(); }
  if (!s_line) return;
  int budget = 2048;                               // bytes per call: never stall the UI
  while (budget-- > 0 && Serial.available()) {
    int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (!s_lineOverflow && s_lineLen > 0 && s_line[0] == '{') {
        s_line[s_lineLen] = 0;
        ProtoCtx ctx{ PV_SERIAL, true };
        String out;
        protoHandle(s_line, s_lineLen, out, ctx);
        Serial.println(out);
      } else if (s_lineOverflow) {
        Serial.println("{\"ok\":false,\"err\":\"line too long\"}");
      }
      s_lineLen = 0; s_lineOverflow = false;
      continue;
    }
    if (s_lineLen < SERIAL_LINE_MAX - 1) s_line[s_lineLen++] = (char)ch;
    else s_lineOverflow = true;
  }
}
