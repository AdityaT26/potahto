// Settings on the device
//
// Categories -> rows. Every row is either a setting from the shared field
// table (Settings.cpp, the same one the web app uses, so both always match)
// or an action / sub-screen (join WiFi, pair Bluetooth, set the time, ...).
//   UP/DOWN pick   LEFT/RIGHT change   SELECT toggle / edit / open
//   hold SELECT    back
// Changes save automatically (~1 s later); the header shows "saved".
#include "Multitool.h"
#include "Proto.h"

// ---- categories
enum { C_WIFI, C_BT, C_SOUND, C_DISPLAY, C_POWER, C_TIME, C_MOUSE, C_CONTROL, C_DESK, C_SENSORS, C_DEVICE, C_N };
struct CatDef { const char* name; uint8_t glyph; const char* groups; };
static const CatDef CATS[C_N] = {
  { "WiFi",            GLYPH_WIFI,     "" },
  { "Bluetooth",       GLYPH_BT,       "" },
  { "Sound & buzz",    GLYPH_MUSIC,    "Sound" },
  { "Display & LED",   GLYPH_LIGHT,    "Display" },
  { "Power & lock",    GLYPH_POWER,    "Power" },
  { "Time & date",     GLYPH_CLOCK,    "" },
  { "Air Mouse",       GLYPH_MOUSE,    "Air Mouse" },
  { "Presenter & keys",GLYPH_SLIDES,   "Presenter|Macro Pad|Shutter|IR Remote" },
  { "Desk",            GLYPH_SHIELD,   "Desk Guard|Pomodoro" },
  { "Sensors",         GLYPH_COMPASS,  "Tripwire|Compass|Scale|Light" },
  { "Device",          GLYPH_INFO,     "" },
};

// ---- actions
enum Act : uint8_t {
  A_NONE, A_WIFI_STATUS, A_WIFI_JOIN, A_WIFI_SAVED, A_WIFI_HIDDEN, A_WIFI_SETUP, A_WIFI_WEB,
  A_BT_STATUS, A_BT_PAIR, A_BT_FORGET,
  A_LOUDTEST, A_SETCODE, A_SCREENOFF, A_LOCKNOW, A_DEEPSLEEP,
  A_TIME_NOW, A_SET_TIME, A_SET_DATE, A_TZ, A_SYNC,
  A_CALGYRO, A_LEVEL, A_CALCOMP, A_SETNORTH, A_RESETCAL,
  A_ABOUT, A_PIN, A_NEWPIN, A_RESTART, A_FACTORY,
};
struct Row { const SettingField* f; uint8_t act; };
#define MAX_ROWS 40
static Row s_rows[MAX_ROWS]; static int s_nRows = 0;

enum Scr : uint8_t { SCR_CATS, SCR_LIST, SCR_SCAN, SCR_SAVED, SCR_CONNECT, SCR_PAIR, SCR_LOUD, SCR_SONG };
static Scr  s_scr = SCR_CATS;
static int  s_cat = 0, s_catSel = 0, s_catTop = 0, s_sel = 0, s_top = 0;
static bool s_netHeld = false;
static uint32_t s_enterAt = 0;
static bool     s_init = false;          // first frame of a sub-screen

static void holdNet(bool on) {
  if (on && !s_netHeld) { if (S.wifiMode == WM_OFF) { S.wifiMode = WM_ON_DEMAND; settingsSaveSoon(); } netAcquire(); s_netHeld = true; }
  if (!on && s_netHeld) { netRelease(); s_netHeld = false; }
}

// ---- rows
static void addAct(uint8_t a) { if (s_nRows < MAX_ROWS) s_rows[s_nRows++] = { nullptr, a }; }
static bool haveField(const SettingField* f) { for (int i = 0; i < s_nRows; i++) if (s_rows[i].f == f) return true; return false; }
static void addField(const char* key) { const SettingField* f = settingsFind(key); if (f && !haveField(f) && s_nRows < MAX_ROWS) s_rows[s_nRows++] = { f, A_NONE }; }
static bool inGroups(const char* groups, const char* g) {
  size_t n = strlen(g);
  for (const char* p = groups; *p; ) {
    const char* e = strchr(p, '|'); size_t L = e ? (size_t)(e - p) : strlen(p);
    if (L == n && !strncmp(p, g, n)) return true;
    if (!e) break; p = e + 1;
  }
  return false;
}
static void addGroups(const char* groups) {
  size_t n; const SettingField* F = settingsFields(&n);
  for (int pass = 0; pass < 2; pass++)                  // normal first, advanced after
    for (size_t i = 0; i < n; i++) {
      const SettingField& f = F[i];
      if (f.flags & (SF_HIDDEN | SF_CUSTOM)) continue;
      if (!strcmp(f.key, "buzRes") || haveField(&f)) continue;   // buzRes: the "Find loudest tone" row sets it
      if (((f.flags & SF_ADV) != 0) != (pass == 1)) continue;
      if (inGroups(groups, f.group) && s_nRows < MAX_ROWS) s_rows[s_nRows++] = { &f, A_NONE };
    }
}
static void buildRows(int cat) {
  s_nRows = 0;
  switch (cat) {
    case C_WIFI:    addAct(A_WIFI_STATUS); addField("wifiMode"); addAct(A_WIFI_JOIN); addAct(A_WIFI_SAVED);
                    addAct(A_WIFI_HIDDEN); addAct(A_WIFI_SETUP); addAct(A_WIFI_WEB); break;
    case C_BT:      addAct(A_BT_STATUS); addField("bleMode"); addField("hidRoute"); addAct(A_BT_PAIR); addAct(A_BT_FORGET); addField("devName"); break;
    case C_SOUND:   addGroups(CATS[cat].groups); addAct(A_LOUDTEST); break;
    case C_POWER:   addAct(A_SCREENOFF); addAct(A_LOCKNOW); addAct(A_DEEPSLEEP); addField("scrOff"); addField("lockMode"); addAct(A_SETCODE);
                    addGroups(CATS[cat].groups); break;
    case C_TIME:    addAct(A_TIME_NOW); addAct(A_SET_TIME); addAct(A_SET_DATE); addField("clock24"); addAct(A_TZ); addAct(A_SYNC); break;
    case C_MOUSE:   addGroups(CATS[cat].groups); addAct(A_CALGYRO); break;
    case C_SENSORS: addGroups(CATS[cat].groups); addAct(A_CALGYRO); addAct(A_LEVEL); addAct(A_CALCOMP); addAct(A_SETNORTH); addAct(A_RESETCAL); break;
    case C_DEVICE:  addAct(A_ABOUT); addField("devName"); addAct(A_PIN); addAct(A_NEWPIN); addAct(A_RESTART); addAct(A_FACTORY); addField("battDiv"); addField("battTrim"); break;
    default:        addGroups(CATS[cat].groups); break;
  }
}

// ---- field text
static void optAt(const char* opts, int idx, char* o, size_t n) {
  o[0] = 0;
  for (int i = 0; opts && *opts; i++) {
    const char* e = strchr(opts, '|'); size_t L = e ? (size_t)(e - opts) : strlen(opts);
    if (i == idx) { size_t k = min(L, n - 1); memcpy(o, opts, k); o[k] = 0; return; }
    if (!e) return; opts = e + 1;
  }
}
static int optCount(const char* o) { int n = 1; for (; o && *o; o++) if (*o == '|') n++; return n; }
// "Screen off after (s)" -> label "Screen off after", unit "s"
static void splitLabel(const char* l, char* lab, size_t ln, char* unit, size_t un) {
  const char* p = strrchr(l, '(');
  unit[0] = 0;
  if (p && l[strlen(l) - 1] == ')') {
    size_t L = p - l; while (L && l[L - 1] == ' ') L--;
    size_t k = min(L, ln - 1); memcpy(lab, l, k); lab[k] = 0;
    size_t u = strlen(p) - 2; u = min(u, un - 1); memcpy(unit, p + 1, u); unit[u] = 0;
  } else strlcpy(lab, l, ln);
}
static const char* songLabel(int v) {
  if (v == 0) return "Default";
  if (v <= SONG_SLOTS) return songName(v)[0] ? songName(v) : "(empty)";
  return builtinName(v - SONG_SLOTS - 1);
}
static void fieldValue(const SettingField& f, char* o, size_t n) {
  char lab[32], unit[8]; splitLabel(f.label, lab, sizeof(lab), unit, sizeof(unit));
  if (f.type == ST_STR) {
    const char* v = (const char*)f.ptr;
    if (f.flags & SF_SECRET) snprintf(o, n, "%s", v[0] ? "set" : "none");
    else snprintf(o, n, "%s", v[0] ? v : "-");
    return;
  }
  double v = settingsGetAsDouble(f);
  if (f.type == ST_BOOL) { if (f.opts) optAt(f.opts, v ? 1 : 0, o, n); else snprintf(o, n, v ? "On" : "Off"); return; }
  if (f.opts) { optAt(f.opts, (int)v, o, n); return; }
  if (f.flags & SF_SONG) { snprintf(o, n, "%s", songLabel((int)v)); return; }
  if ((f.flags & SF_ZOFF) && v == 0) { snprintf(o, n, "Off"); return; }
  if (f.type == ST_F32) snprintf(o, n, f.step < 0.1f ? "%.2f%s%s" : "%.0f%s%s", v, unit[0] ? " " : "", unit);
  else snprintf(o, n, "%ld%s%s", (long)v, unit[0] && strcmp(unit, "%") ? " " : "", unit);
}
static void fieldStep(const SettingField& f, int dir, bool fast) {
  if (f.type == ST_STR || f.type == ST_BLOB) return;
  double v = settingsGetAsDouble(f);
  if (f.type == ST_BOOL) v = v ? 0 : 1;
  else if (f.opts) { int c = optCount(f.opts); v = ((int)v + dir + c) % c; }
  else if (f.flags & SF_SONG) {
    int x = (int)v;
    for (int k = 0; k < SONG_SLOTS + builtinCount() + 1; k++) {
      x = (x + dir + SONG_SLOTS + builtinCount() + 1) % (SONG_SLOTS + builtinCount() + 1);
      if (x == 0 || x > SONG_SLOTS || songName(x)[0]) break;
    }
    v = x;
  } else {
    double st = f.step > 0 ? f.step : 1;
    v += dir * st * (fast ? 5 : 1);
    if (v < f.lo) v = f.lo; if (v > f.hi) v = f.hi;
  }
  settingsSetFromDouble(f, v);
  protoApplySideEffects(f.key);
  settingsSaveSoon();
}

// ---- editors
static const SettingField* s_editF = nullptr;
static int  s_editInt = 0;
static char s_editBuf[128];
static void sliderChange(int v) { if (s_editF) { settingsSetFromDouble(*s_editF, v * (s_editF->type == ST_F32 ? s_editF->step : 1)); protoApplySideEffects(s_editF->key); } }
static void sliderDone() { settingsSaveSoon(); }
static void textDone(bool ok) {
  if (!ok || !s_editF) return;
  if (s_editF->flags & SF_MACRO) {
    char err[48];
    if (s_editBuf[0] && !hidCheckMacro(s_editBuf, err, sizeof(err))) { toast(err); fbError(); return; }
  }
  if (!strcmp(s_editF->key, "devName") && !s_editBuf[0]) { toast("Name can't be empty"); return; }
  if (settingsSetFromString(*s_editF, s_editBuf)) {
    protoApplySideEffects(s_editF->key); settingsSave();
    toast(s_editF->flags & SF_REBOOT ? "Saved - restart to apply" : "Saved");
  } else { toast("Too long"); fbError(); }
}
static void editField(const SettingField& f) {
  s_editF = &f;
  if (f.type == ST_STR) {
    strlcpy(s_editBuf, (const char*)f.ptr, sizeof(s_editBuf));
    char lab[32], unit[8]; splitLabel(f.label, lab, sizeof(lab), unit, sizeof(unit));
    kbdOpen(lab, s_editBuf, min<size_t>(f.size, sizeof(s_editBuf)), (f.flags & SF_SECRET) != 0, textDone);
    return;
  }
  if (f.type == ST_BOOL || f.opts) { fieldStep(f, +1, false); fbNav(); return; }
  if (f.flags & SF_SONG) { s_scr = SCR_SONG; s_init = true; return; }
  char lab[32], unit[8]; splitLabel(f.label, lab, sizeof(lab), unit, sizeof(unit));
  float st = f.type == ST_F32 ? f.step : 1;
  s_editInt = (int)lround(settingsGetAsDouble(f) / st);
  int stepI = f.type == ST_F32 ? 1 : (int)max(1.0f, f.step);
  modalSlider(lab, &s_editInt, (int)lround(f.lo / st), (int)lround(f.hi / st), stepI, f.type == ST_F32 ? "" : unit, sliderChange, sliderDone);
}

// ---- WiFi flows
static char s_ssid[33], s_user[65], s_pass[65];
static uint8_t s_eap = 0;
static uint32_t s_connAt = 0;
static int s_scanSel = 0, s_scanTop = 0, s_savedSel = 0;
static void startConnect() {
  netSaveNetwork(s_ssid, s_pass, s_user, s_eap);
  holdNet(true);
  s_scr = SCR_CONNECT; s_connAt = millis();
}
static void passDone(bool ok) { if (ok) startConnect(); }
static void userDone(bool ok) { if (!ok) return; s_pass[0] = 0; kbdOpen("Password", s_pass, sizeof(s_pass), true, passDone); }
static void askCredentials(bool enterprise, bool open) {
  s_user[0] = 0; s_pass[0] = 0; s_eap = enterprise ? 1 : 0;
  if (open) { startConnect(); return; }
  if (enterprise) kbdOpen("Username (campus ID)", s_user, sizeof(s_user), false, userDone);
  else kbdOpen("Password", s_pass, sizeof(s_pass), true, passDone);
}
static void hiddenSecDone(int pick) { askCredentials(pick == 2, pick == 0); }
static void hiddenSsidDone(bool ok) {
  if (!ok || !s_ssid[0]) return;
  static const char* const T[] = { "Open (no password)", "Password (WPA2/3)", "Username + password" };
  modalChoice("Security", T, 3, 1, hiddenSecDone);
}
static void alwaysYes() { S.wifiMode = WM_ALWAYS; settingsSave(); netReconnect(); toast("WiFi stays on"); }
static void savedPick(int a) {
  int n = 0, slot = -1;
  for (int i = 0; i < WIFI_SLOTS; i++) if (S.wifiSsid[i][0]) { if (n == s_savedSel) slot = i; n++; }
  if (slot < 0) return;
  if (a == 0) { strlcpy(s_ssid, S.wifiSsid[slot], sizeof(s_ssid)); netConnectSlot(slot); holdNet(true); s_scr = SCR_CONNECT; s_connAt = millis(); }
  else if (a == 1) { strlcpy(s_ssid, S.wifiSsid[slot], sizeof(s_ssid)); askCredentials(S.wifiEap[slot] != 0, false); }
  else if (a == 2) { netForget(slot); toast("Forgotten"); if (s_savedSel) s_savedSel--; }
}

// ---- time
static int s_tH, s_tM, s_tD, s_tMo, s_tY;
static void curDate() {
  struct tm lt;
  if (timeNow(lt)) { s_tY = lt.tm_year + 1900; s_tMo = lt.tm_mon + 1; s_tD = lt.tm_mday; s_tH = lt.tm_hour; s_tM = lt.tm_min; }
  else { s_tY = 2026; s_tMo = 1; s_tD = 1; s_tH = 12; s_tM = 0; }
}
static void timeDoneM() { timeSetLocal(s_tY, s_tMo, s_tD, s_tH, s_tM); toast("Time set"); }
static void timeDoneH() { modalSlider("Minute", &s_tM, 0, 59, 1, "", nullptr, timeDoneM); }
static void dateDoneY() { timeSetLocal(s_tY, s_tMo, s_tD, s_tH, s_tM); toast("Date set"); }
static void dateDoneMo() { modalSlider("Year", &s_tY, 2024, 2099, 1, "", nullptr, dateDoneY); }
static void dateDoneD() { modalSlider("Month", &s_tMo, 1, 12, 1, "", nullptr, dateDoneMo); }
struct Tz { const char* name; const char* tz; };
static const Tz TZS[] = {
  { "India (IST)",        "IST-5:30" },
  { "UTC",                "UTC0" },
  { "London",             "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Berlin / Paris",     "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Dubai",              "<+04>-4" },
  { "Singapore",          "<+08>-8" },
  { "Tokyo",              "JST-9" },
  { "Sydney",             "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "New York",           "EST5EDT,M3.2.0,M11.1.0" },
  { "Chicago",            "CST6CDT,M3.2.0,M11.1.0" },
  { "Denver",             "MST7MDT,M3.2.0,M11.1.0" },
  { "Los Angeles",        "PST8PDT,M3.2.0,M11.1.0" },
};
static void tzPick(int i) { strlcpy(S.tz, TZS[i].tz, sizeof(S.tz)); timeApplyTz(); settingsSave(); toast(TZS[i].name); }

// ---- actions
static void doResetCal() {
  for (int k = 0; k < 3; k++) { S.gyroBias[k] = 0; S.accelBias[k] = 0; }
  compassCalReset();
  S.mpuCalibrated = false; S.compassCalibrated = false; S.magOffset = 0;
  settingsSave(); toast("Cal cleared");
}
static void doFactory() { settingsFactoryReset(); uiSetRotation(S.rotate180); timeApplyTz(); toast("Reset done"); }
static void codeSet(const char* c) {
  if (!c) return;
  strlcpy(S.lockCode, c, sizeof(S.lockCode)); S.lockMode = 2; settingsSave(); toast("Code lock on");
}
static void newPin() {
  snprintf(S.webPin, sizeof(S.webPin), "%06lu", (unsigned long)(esp_random() % 1000000UL));
  settingsSave(); char m[32]; snprintf(m, sizeof(m), "New PIN %s", S.webPin); modalInfo("Web PIN", m);
}

static void actLabel(uint8_t a, char* lab, size_t ln, char* val, size_t vn) {
  val[0] = 0;
  struct tm lt;
  switch (a) {
    case A_WIFI_STATUS: snprintf(lab, ln, "Status"); snprintf(val, vn, "%s", netOnline() ? netSsid() : netStateName()); break;
    case A_WIFI_JOIN:   snprintf(lab, ln, "Join a network"); snprintf(val, vn, ">"); break;
    case A_WIFI_SAVED:  { int n = 0; for (int i = 0; i < WIFI_SLOTS; i++) if (S.wifiSsid[i][0]) n++; snprintf(lab, ln, "Saved networks"); snprintf(val, vn, "%d >", n); } break;
    case A_WIFI_HIDDEN: snprintf(lab, ln, "Hidden network"); snprintf(val, vn, ">"); break;
    case A_WIFI_SETUP:  snprintf(lab, ln, "Setup hotspot"); snprintf(val, vn, netSetupAPActive() ? "On" : "Off"); break;
    case A_WIFI_WEB:    snprintf(lab, ln, "Web app address"); snprintf(val, vn, ">"); break;
    case A_BT_STATUS:   snprintf(lab, ln, "Status");
                        if (hidBleConnected()) snprintf(val, vn, "%s", blePeerName()[0] ? blePeerName() : "connected");
                        else snprintf(val, vn, "%s", hidBleStarted() ? (hidBondCount() ? "waiting" : "not paired") : "off"); break;
    case A_BT_PAIR:     snprintf(lab, ln, "Pair a new device"); snprintf(val, vn, ">"); break;
    case A_BT_FORGET:   snprintf(lab, ln, "Forget pairings"); snprintf(val, vn, "%d", hidBondCount()); break;
    case A_LOUDTEST:    snprintf(lab, ln, "Find loudest tone"); snprintf(val, vn, "%u Hz", S.buzResHz); break;
    case A_SETCODE:     snprintf(lab, ln, "Set unlock code"); snprintf(val, vn, S.lockCode[0] ? "set" : "none"); break;
    case A_SCREENOFF:   snprintf(lab, ln, "Screen off now"); break;
    case A_LOCKNOW:     snprintf(lab, ln, "Lock now"); break;
    case A_DEEPSLEEP:   snprintf(lab, ln, "Deep sleep now"); break;
    case A_TIME_NOW:    snprintf(lab, ln, "Now");
                        if (timeNow(lt)) { char hm[8]; timeFmtHM(hm, sizeof(hm), lt); snprintf(val, vn, "%s%s %s", hm, timeAmPm(lt), timeSource()); }
                        else snprintf(val, vn, "not set"); break;
    case A_SET_TIME:    snprintf(lab, ln, "Set time"); snprintf(val, vn, ">"); break;
    case A_SET_DATE:    snprintf(lab, ln, "Set date"); if (timeNow(lt)) strftime(val, vn, "%d %b %Y", &lt); break;
    case A_TZ:          snprintf(lab, ln, "Time zone"); snprintf(val, vn, "%s", S.tz); for (auto& z : TZS) if (!strcmp(z.tz, S.tz)) snprintf(val, vn, "%s", z.name); break;
    case A_SYNC:        snprintf(lab, ln, "Sync over WiFi"); snprintf(val, vn, netOnline() ? "online" : ">"); break;
    case A_CALGYRO:     snprintf(lab, ln, "Calibrate gyro"); snprintf(val, vn, S.mpuCalibrated ? "done" : ">"); break;
    case A_LEVEL:       snprintf(lab, ln, "Set level here"); break;
    case A_CALCOMP:     snprintf(lab, ln, "Calibrate compass"); snprintf(val, vn, S.compassCalibrated ? "done" : ">"); break;
    case A_SETNORTH:    snprintf(lab, ln, "Set north here"); break;
    case A_RESETCAL:    snprintf(lab, ln, "Reset calibration"); break;
    case A_ABOUT:       snprintf(lab, ln, "About"); snprintf(val, vn, "v" FW_VERSION); break;
    case A_PIN:         snprintf(lab, ln, "Web PIN"); snprintf(val, vn, "%s", S.webPin); break;
    case A_NEWPIN:      snprintf(lab, ln, "New web PIN"); break;
    case A_RESTART:     snprintf(lab, ln, "Restart"); break;
    case A_FACTORY:     snprintf(lab, ln, "Factory reset"); break;
    default:            lab[0] = 0;
  }
}
static void runAct(uint8_t a) {
  switch (a) {
    case A_WIFI_STATUS: {
      char m[44];
      if (netOnline()) snprintf(m, sizeof(m), "%s\n%s  %d dBm", netSsid(), netIp(), netRssi());
      else snprintf(m, sizeof(m), "%s\nmode: %s", netStateName(), S.wifiMode == WM_OFF ? "off" : S.wifiMode == WM_ALWAYS ? "always on" : "on demand");
      modalInfo("WiFi", m);
    } break;
    case A_WIFI_JOIN:   holdNet(true); netScanStart(); s_scanSel = 0; s_scanTop = 0; s_scr = SCR_SCAN; s_enterAt = millis(); break;
    case A_WIFI_SAVED:  s_savedSel = 0; s_scr = SCR_SAVED; break;
    case A_WIFI_HIDDEN: s_ssid[0] = 0; kbdOpen("Network name (SSID)", s_ssid, sizeof(s_ssid), false, hiddenSsidDone); break;
    case A_WIFI_SETUP:  netSetupAP(!netSetupAPActive()); toast(netSetupAPActive() ? "Hotspot on: see Connect" : "Hotspot off"); break;
    case A_WIFI_WEB:    openApp(APP_CONNECT); break;
    case A_BT_STATUS: {
      char m[44];
      snprintf(m, sizeof(m), "%s\nkeys go to: %s", hidBleConnected() ? (blePeerName()[0] ? blePeerName() : "Connected") : "Not connected", hidRouteName());
      modalInfo("Bluetooth", m);
    } break;
    case A_BT_PAIR:     hidBleAdvertise(); s_scr = SCR_PAIR; s_enterAt = millis(); break;
    case A_BT_FORGET:   modalConfirm("Bluetooth", "Forget every pairing?", []() { hidUnpairAll(); toast("Pairings cleared"); }); break;
    case A_LOUDTEST:    s_scr = SCR_LOUD; s_init = true; break;
    case A_SETCODE:     codeOpen("Unlock code", codeSet); break;
    case A_SCREENOFF:   powerScreenOff(false); break;
    case A_LOCKNOW:     powerLockNow(); break;
    case A_DEEPSLEEP:   modalConfirm("Deep sleep", "UP/DOWN/LEFT wakes it", []() { powerDeepSleep(); }); break;
    case A_TIME_NOW:    { char m[44]; snprintf(m, sizeof(m), "source: %s\nzone: %s", timeValid() ? timeSource() : "none yet", S.tz); modalInfo("Clock", m); } break;
    case A_SET_TIME:    curDate(); modalSlider("Hour", &s_tH, 0, 23, 1, "", nullptr, timeDoneH); break;
    case A_SET_DATE:    curDate(); modalSlider("Day", &s_tD, 1, 31, 1, "", nullptr, dateDoneD); break;
    case A_TZ: {
      static const char* names[ARRAY_LEN(TZS)]; int cur = 0;
      for (size_t i = 0; i < ARRAY_LEN(TZS); i++) { names[i] = TZS[i].name; if (!strcmp(S.tz, TZS[i].tz)) cur = i; }
      modalChoice("Time zone", names, ARRAY_LEN(TZS), cur, tzPick);
    } break;
    case A_SYNC:        if (!S.wifiSsid[0][0]) toast("Save a WiFi network first"); else { holdNet(true); toast("Syncing over WiFi"); } break;
    case A_CALGYRO:     if (mpuPresent()) { toast("Hold still..."); mpuCalibrateGyro(); fbConfirm(); toast("Gyro zeroed"); } else fbError(); break;
    case A_LEVEL:       if (mpuPresent()) { mpuLevelHere(); fbConfirm(); toast("Leveled"); } else fbError(); break;
    case A_CALCOMP:     if (compassPresent()) { fbSelect(); openApp(APP_COMPASS); } else fbError(); break;
    case A_SETNORTH:    if (compassPresent()) { compassSetNorth(); fbConfirm(); toast("North set"); } else fbError(); break;
    case A_RESETCAL:    modalConfirm("Reset cal", "Clear all calibration?", doResetCal); break;
    case A_ABOUT: {
      char m[44]; snprintf(m, sizeof(m), FW_NAME " v" FW_VERSION "\nup %lum  heap %uk", (unsigned long)(millis() / 60000), (unsigned)(ESP.getFreeHeap() / 1024));
      modalInfo("About", m);
    } break;
    case A_PIN:         { char m[40]; snprintf(m, sizeof(m), "PIN %s\nweb app + OTA", S.webPin); modalInfo("Web PIN", m); } break;
    case A_NEWPIN:      modalConfirm("Web PIN", "Make a new PIN?", newPin); break;
    case A_RESTART:     modalConfirm("Restart", "Restart now?", []() { powerRestart(); }); break;
    case A_FACTORY:     modalConfirm("Factory", "Erase ALL settings?", doFactory); break;
  }
}

// ---- drawing helpers
static void header(const char* t) {
  uiHeader(t);
  if (settingsPending() || settingsSavedAgoMs() < 1500) {     // little "saved" tick
    u8g2.setDrawColor(0); u8g2.drawBox(56, 0, 28, 10); u8g2.setDrawColor(1);
    u8g2.setFont(F_TINY);
    if (settingsPending()) u8g2.drawStr(60, 7, "...");
    else { u8g2.drawStr(62, 7, "saved"); }
  }
}
static void row(int slot, const char* label, const char* value, bool sel) {
  int top = BODY_Y + slot * 16;
  if (sel) { u8g2.drawRBox(0, top, 125, 15, 2); u8g2.setDrawColor(0); }
  u8g2.setFont(F_SMALL);
  char v[26]; uiTruncate(v, sizeof(v), value, 64);
  int vw = v[0] ? u8g2.getStrWidth(v) : 0;
  if (vw) u8g2.drawStr(122 - vw, top + 11, v);
  u8g2.setFont(F_UI);
  int maxW = 118 - vw - (vw ? 5 : 0);
  int lw = u8g2.getStrWidth(label);
  if (sel && lw > maxW) {                          // scroll the selected label
    int span = lw - maxW + 12, ph = (int)((millis() / 45) % (span + 40));
    int off = ph < 20 ? 0 : ph < 20 + span ? ph - 20 : span;
    u8g2.setClipWindow(4, top, 4 + maxW, top + 15);
    u8g2.drawStr(5 - off, top + 11, label);
    u8g2.setMaxClipWindow();
  } else uiTextClipped(5, top + 11, label, maxW);
  if (sel) u8g2.setDrawColor(1);
}
static void listNav(int& sel, int& top, int n, int rows) {
  if (btnRepeat(BTN_DOWN)) { if (sel < n - 1) { sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (sel > 0) { sel--; fbNav(); } else fbEdge(); }
  if (sel < top) top = sel;
  if (sel >= top + rows) top = sel - rows + 1;
}

// ---- screens
static void catsLoop() {
  if (wantBack()) { holdNet(false); fbBack(); goBack(); return; }
  const int rows = 3;
  listNav(s_catSel, s_catTop, C_N, rows);
  if (btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) { s_cat = s_catSel; buildRows(s_cat); s_sel = 0; s_top = 0; s_scr = SCR_LIST; fbSelect(); return; }
  header("Settings");
  for (int r = 0; r < rows; r++) {
    int i = s_catTop + r; if (i >= C_N) break;
    int top = BODY_Y + r * 17; bool sel = i == s_catSel;
    if (sel) { u8g2.drawRBox(0, top, 125, 16, 2); u8g2.setDrawColor(0); }
    uiGlyph(CATS[i].glyph, 3, top + 1, 14);
    u8g2.setFont(F_UI); uiTextClipped(22, top + 12, CATS[i].name, 100);
    if (sel) u8g2.setDrawColor(1);
  }
  uiScrollbar(C_N, s_catTop, rows);
}

static void listLoop() {
  if (wantBack()) { s_scr = SCR_CATS; holdNet(false); fbBack(); return; }
  if (s_cat == C_WIFI || s_cat == C_BT || s_cat == C_TIME) { /* rows are live */ }
  const int rows = 3;
  listNav(s_sel, s_top, s_nRows, rows);
  const Row& R = s_rows[s_sel];
  static uint32_t s_repeatSince = 0;
  bool lr = btnDown(BTN_LEFT) || btnDown(BTN_RIGHT);
  if (!lr) s_repeatSince = 0; else if (!s_repeatSince) s_repeatSince = millis();
  bool fast = s_repeatSince && millis() - s_repeatSince > 1500;
  if (R.f) {
    if (btnRepeat(BTN_RIGHT)) { fieldStep(*R.f, +1, fast); fbNav(); }
    if (btnRepeat(BTN_LEFT))  { fieldStep(*R.f, -1, fast); fbNav(); }
    if (btnTap(BTN_SELECT)) editField(*R.f);
  } else if (btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) { fbSelect(); runAct(R.act); return; }
  header(CATS[s_cat].name);
  for (int r = 0; r < rows; r++) {
    int i = s_top + r; if (i >= s_nRows) break;
    char lab[40], val[40];
    if (s_rows[i].f) { char unit[8]; splitLabel(s_rows[i].f->label, lab, sizeof(lab), unit, sizeof(unit)); fieldValue(*s_rows[i].f, val, sizeof(val)); }
    else actLabel(s_rows[i].act, lab, sizeof(lab), val, sizeof(val));
    row(r, lab, val, i == s_sel);
  }
  uiScrollbar(s_nRows, s_top, rows);
}

static void scanLoop() {
  if (wantBack()) { s_scr = SCR_LIST; fbBack(); return; }
  int n; const NetAP* a = netScanResults(&n);
  // unique SSIDs, strongest first (the scan is already sorted)
  static int idx[NET_MAX_APS]; int m = 0;
  for (int i = 0; i < n && m < NET_MAX_APS; i++) {
    if (!a[i].ssid[0]) continue;
    bool dup = false; for (int k = 0; k < m; k++) if (!strcmp(a[idx[k]].ssid, a[i].ssid)) dup = true;
    if (!dup) idx[m++] = i;
  }
  const int rows = 3;
  listNav(s_scanSel, s_scanTop, m, rows);
  if (btnTap(BTN_RIGHT) && n != -1) { netScanStart(); fbNav(); }
  if (btnTap(BTN_SELECT) && m) {
    const NetAP& ap = a[idx[s_scanSel]];
    strlcpy(s_ssid, ap.ssid, sizeof(s_ssid));
    askCredentials(netEncEnterprise(ap.enc), netEncOpen(ap.enc));
    fbSelect();
    return;
  }
  header("Join a network");
  if (n == -1 || n == -2) { u8g2.setFont(F_SMALL); uiCenter(36, n == -1 ? "Scanning..." : "Starting WiFi..."); if (n == -2 && netState() != NET_OFF && millis() - s_enterAt > 1500) netScanStart(); return; }
  if (!m) { u8g2.setFont(F_SMALL); uiCenter(34, "No networks found"); u8g2.setFont(F_TINY); uiCenter(46, "RIGHT to scan again"); return; }
  for (int r = 0; r < rows; r++) {
    int i = s_scanTop + r; if (i >= m) break;
    const NetAP& ap = a[idx[i]];
    char v[16]; snprintf(v, sizeof(v), "%s%d", netEncEnterprise(ap.enc) ? "ID " : netEncOpen(ap.enc) ? "open " : "", ap.rssi);
    row(r, ap.ssid, v, i == s_scanSel);
  }
  uiScrollbar(m, s_scanTop, rows);
  u8g2.setFont(F_TINY);
}

static void savedLoop() {
  if (wantBack()) { s_scr = SCR_LIST; fbBack(); return; }
  int slots[WIFI_SLOTS], m = 0;
  for (int i = 0; i < WIFI_SLOTS; i++) if (S.wifiSsid[i][0]) slots[m++] = i;
  static int top = 0;
  listNav(s_savedSel, top, m, 3);
  if (btnTap(BTN_SELECT) && m) {
    static const char* const A[] = { "Connect now", "Change password", "Forget" };
    modalChoice(S.wifiSsid[slots[s_savedSel]], A, 3, 0, savedPick);
    return;
  }
  header("Saved networks");
  if (!m) { u8g2.setFont(F_SMALL); uiCenter(36, "None saved yet"); return; }
  int cur = netCurrentSlot();
  for (int r = 0; r < 3; r++) {
    int i = top + r; if (i >= m) break;
    int sl = slots[i];
    row(r, S.wifiSsid[sl], sl == cur ? "online" : S.wifiEap[sl] ? "campus ID" : "", i == s_savedSel);
  }
  uiScrollbar(m, top, 3);
}

static void connectLoop2() {
  uint32_t el = millis() - s_connAt;
  bool ok = netOnline() && !strcmp(netSsid(), s_ssid);
  if (wantBack() || (ok && el > 600)) {
    s_scr = SCR_LIST;
    if (ok) { fbConfirm(); toast("Connected"); if (S.wifiMode == WM_ON_DEMAND) modalConfirm("Stay online", "Keep WiFi always on?", alwaysYes); }
    return;
  }
  header("Connecting");
  u8g2.setFont(F_UI); uiTextClipped(2, 26, s_ssid, 124);
  u8g2.setFont(F_SMALL);
  bool failed = el > 30000 || (netState() == NET_FAILED && el > 4000);
  if (ok) uiCenter(42, "Connected!");
  else if (failed) { uiCenter(40, "Couldn't connect."); u8g2.setFont(F_TINY); uiCenter(50, "Check the password / username,"); uiCenter(57, "or move closer. Hold SEL: back"); }
  else {
    uiCenter(40, netStateName());
    for (int i = 0; i < 5; i++) { int x = 44 + i * 9; if (((millis() / 150) % 5) == (uint32_t)i) u8g2.drawBox(x, 47, 6, 6); else u8g2.drawFrame(x, 47, 6, 6); }
  }
}

static void pairLoop() {
  if (wantBack()) { s_scr = SCR_LIST; fbBack(); return; }
  header("Pair");
  if (hidBleConnected() && millis() - s_enterAt > 1000) {
    u8g2.setFont(F_UI); uiCenter(30, "Connected!");
    u8g2.setFont(F_SMALL); uiCenter(44, blePeerName()[0] ? blePeerName() : "");
    u8g2.setFont(F_TINY); uiCenter(60, "hold SELECT: back");
    return;
  }
  uiGlyph(GLYPH_BT, 2, 16, 24);
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(30, 22, "On your PC / phone:");
  u8g2.drawStr(30, 31, "Bluetooth > Add device");
  char l[40]; snprintf(l, sizeof(l), "pick \"%s\"", S.devName); u8g2.drawStr(30, 40, l);
  u8g2.setFont(F_TINY);
  uiCenter(52, hidBleAdvertising() ? "discoverable now" : "starting...");
  uiCenter(62, "already paired elsewhere? forget it there");
}

static uint16_t s_loudF = 1500; static bool s_loudSweep = true; static uint32_t s_loudStep = 0;
static void loudLoop() {
  if (wantBack()) { buzzAlarmStop(); s_scr = SCR_LIST; fbBack(); return; }
  uint32_t now = millis();
  if (s_init) { s_init = false; s_loudF = 1500; s_loudSweep = true; s_loudStep = now; }
  if (s_loudSweep) {
    if (now - s_loudStep > 220) { s_loudStep = now; s_loudF += 100; if (s_loudF > 5000) s_loudF = 1500; }
    if (btnTap(BTN_SELECT)) { s_loudSweep = false; fbNav(); }
  } else {
    if (btnRepeat(BTN_RIGHT)) { s_loudF = min(5000, s_loudF + 50); }
    if (btnRepeat(BTN_LEFT))  { s_loudF = max(1500, s_loudF - 50); }
    if (btnTap(BTN_UP)) { s_loudSweep = true; }
    if (btnTap(BTN_SELECT)) { S.buzResHz = s_loudF; settingsSave(); buzzAlarmStop(); toast("Saved: alarms use it"); s_scr = SCR_LIST; return; }
  }
  buzzTestTone(s_loudF);
  header("Loudest tone");
  char f[12]; snprintf(f, sizeof(f), "%u", s_loudF);
  uiBigValue(64, 40, f, "Hz");
  uiProgressBar(10, 45, 108, 6, (s_loudF - 1500) / 3500.0f);
  u8g2.setFont(F_TINY);
  uiCenter(62, s_loudSweep ? "SELECT when it's loudest" : "</> tune  SEL save  ^ again");
}

static int s_songSel = 0, s_songTop = 0;
static void songLoop() {
  if (!s_editF) { s_scr = SCR_LIST; return; }
  // choices: 0 default, user songs, built-ins
  static int vals[1 + SONG_SLOTS + 24]; int m = 0;
  vals[m++] = 0;
  for (int i = 1; i <= SONG_SLOTS; i++) if (songName(i)[0]) vals[m++] = i;
  for (int i = 0; i < builtinCount() && m < (int)ARRAY_LEN(vals); i++) vals[m++] = SONG_SLOTS + 1 + i;
  if (s_init) { s_init = false; s_songSel = 0; for (int i = 0; i < m; i++) if (vals[i] == (int)settingsGetAsDouble(*s_editF)) s_songSel = i; s_songTop = max(0, s_songSel - 1); }
  if (wantBack() || btnTap(BTN_LEFT)) { songStop(); s_scr = SCR_LIST; fbBack(); return; }
  listNav(s_songSel, s_songTop, m, 3);
  if (btnTap(BTN_RIGHT)) {
    int v = vals[s_songSel];
    if (!v) { songStop(); fbConfirm(); }
    else if (v <= SONG_SLOTS) songPlaySlot(v, false);
    else songPlayRtttl(builtinRtttl(v - SONG_SLOTS - 1), false);
  }
  if (btnTap(BTN_SELECT)) {
    songStop();
    settingsSetFromDouble(*s_editF, vals[s_songSel]); settingsSaveSoon(); fbConfirm();
    s_scr = SCR_LIST; return;
  }
  char lab[32], unit[8]; splitLabel(s_editF->label, lab, sizeof(lab), unit, sizeof(unit));
  header(lab);
  for (int r = 0; r < 3; r++) {
    int i = s_songTop + r; if (i >= m) break;
    row(r, songLabel(vals[i]), vals[i] > SONG_SLOTS ? "built-in" : "", i == s_songSel);
  }
  uiScrollbar(m, s_songTop, 3);
}

void settingsAppEnter() { s_scr = SCR_CATS; }
void settingsAppLoop() {
  switch (s_scr) {
    case SCR_CATS:    catsLoop(); break;
    case SCR_LIST:    listLoop(); break;
    case SCR_SCAN:    scanLoop(); break;
    case SCR_SAVED:   savedLoop(); break;
    case SCR_CONNECT: connectLoop2(); break;
    case SCR_PAIR:    pairLoop(); break;
    case SCR_LOUD:    loudLoop(); break;
    case SCR_SONG:    songLoop(); break;
  }
}

// open Settings straight at a category (e.g. from the WiFi icon / Power app)
void settingsOpenCategory(const char* name) {
  for (int i = 0; i < C_N; i++) if (!strcmp(CATS[i].name, name)) {
    openApp(APP_SETTINGS);
    s_cat = s_catSel = i; buildRows(i); s_sel = s_top = 0; s_scr = SCR_LIST;
    return;
  }
}
