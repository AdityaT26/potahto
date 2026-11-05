// Profile storage, defaults, protocol commands (macro.*, hid.*)
#include "Multitool.h"
#include "Proto.h"

#define MACRO_FILE "/macros.json"

// Starter profiles (Windows shortcuts; edit freely in the web app, where a
// library of more app profiles is one click away).
static const char DEFAULTS[] PROGMEM = R"json({"v":2,"profiles":[
{"name":"Windows","layout":"hold","app":"explorer.exe","keys":[["Task view","{win+tab}"],["Desktop","{win+d}"],["Alt-Tab","{alt+tab}"],["Snip","{win+shift+s}{pointer drag}"],["Clipboard","{win+v}"],
  ["Lock","{win+l}"],["Explorer","{win+e}"],["Settings","{win+i}"],["Flush DNS","{admin ipconfig /flushdns}"]]},
{"name":"Media","layout":"hold","app":"","keys":[["Vol +","{media volup}"],["Vol -","{media voldown}"],["Prev","{media prev}"],["Next","{media next}"],["Play","{media playpause}"],
  ["Mute","{media mute}"],["Stop","{media stop}"],["-5 s","{left}"],["+5 s","{right}"]]},
{"name":"VS Code","layout":"folders","app":"Code.exe","keys":[["Commands","{ctrl+shift+p}"],["Terminal","{ctrl+`}"],["Back","{alt+left}"],["Forward","{alt+right}"],["Files","{ctrl+p}"],
  ["Edit",""],["Run",""],["View",""],["Go to",""],
  ["Comment","{ctrl+/}"],["Format","{shift+alt+f}"],["Rename","{f2}"],["Multi-cur","{ctrl+alt+down}"],["Fold","{ctrl+shift+[}"],
  ["Run","{f5}"],["Stop","{shift+f5}"],["Step into","{f11}"],["Step over","{f10}"],["Breakpoint","{f9}"],
  ["Explorer","{ctrl+shift+e}"],["Search","{ctrl+shift+f}"],["Git","{ctrl+shift+g}"],["Split","{ctrl+\\}"],["Zen","{ctrl+k}z"],
  ["Line","{ctrl+g}"],["Symbol","{ctrl+shift+o}"],["References","{shift+f12}"],["Definition","{f12}"],["Problems","{ctrl+shift+m}"]]},
{"name":"Chrome","layout":"folders","app":"chrome.exe;msedge.exe","keys":[["New tab","{ctrl+t}"],["Close tab","{ctrl+w}"],["Prev tab","{ctrl+shift+tab}"],["Next tab","{ctrl+tab}"],["Reopen","{ctrl+shift+t}"],
  ["Page",""],["Navigate",""],["Window",""],["Video",""],
  ["Top","{home}"],["Bottom","{end}"],["Zoom -","{ctrl+minus}"],["Zoom +","{ctrl+equal}"],["Reload","{f5}"],
  ["Address","{ctrl+l}"],["Downloads","{ctrl+j}"],["Back","{alt+left}"],["Forward","{alt+right}"],["Find","{ctrl+f}"],
  ["New window","{ctrl+n}"],["Incognito","{ctrl+shift+n}"],["History","{ctrl+h}"],["Bookmark","{ctrl+d}"],["Full scrn","{f11}"],
  ["Play/pause","k"],["Mute","m"],["Back 10 s","j"],["Fwd 10 s","l"],["Fullscreen","f"]]},
{"name":"PowerPoint","layout":"hold","app":"POWERPNT.EXE","keys":[["Start","{f5}"],["From here","{shift+f5}"],["Prev","{left}"],["Next","{right}"],["Black","b"],
  ["Laser","{ctrl+l}"],["Pen","{ctrl+p}"],["Erase ink","e"],["End show","{esc}"]]},
{"name":"Teams","layout":"hold","app":"ms-teams.exe;Teams.exe","keys":[["Mute","{ctrl+shift+m}"],["Camera","{ctrl+shift+o}"],["Hand","{ctrl+shift+k}"],["Share","{ctrl+shift+e}"],["Accept","{ctrl+shift+s}"],
  ["Chat","{ctrl+2}"],["Leave","{ctrl+shift+h}"],["Blur","{ctrl+shift+p}"],["Decline","{ctrl+shift+d}"]]},
{"name":"Zoom","layout":"hold","app":"Zoom.exe","keys":[["Mute","{alt+a}"],["Video","{alt+v}"],["Hand","{alt+y}"],["Share","{alt+s}"],["Chat","{alt+h}"],
  ["Record","{alt+r}"],["Leave","{alt+q}"],["Gallery","{alt+f2}"],["Speaker","{alt+f1}"]]}
]})json";

static char s_names[MACRO_PROFILES][MACRO_NAME_MAX + 1];
static int  s_count = 0;

static bool loadDoc(JsonDocument& doc) {
  String txt;
  if (storeRead(MACRO_FILE, txt) && !deserializeJson(doc, txt) && doc["profiles"].is<JsonArray>()) return true;
  doc.clear();
  return !deserializeJson(doc, DEFAULTS);
}
static void cacheNames() {
  JsonDocument doc; loadDoc(doc);
  s_count = 0;
  for (JsonObjectConst p : doc["profiles"].as<JsonArrayConst>()) {
    if (s_count >= MACRO_PROFILES) break;
    strlcpy(s_names[s_count], p["name"] | "Profile", sizeof(s_names[0]));
    s_count++;
  }
}
int         macroProfileCount()     { return s_count; }
const char* macroProfileName(int i) { return (i >= 0 && i < s_count) ? s_names[i] : ""; }

bool macroLoadProfile(int idx, MacroProfile& out) {
  JsonDocument doc; loadDoc(doc);
  JsonArrayConst ps = doc["profiles"];
  if (idx < 0 || idx >= (int)ps.size()) return false;
  JsonObjectConst p = ps[idx];
  strlcpy(out.name, p["name"] | "Profile", sizeof(out.name));
  out.layout = !strcmp(p["layout"] | "hold", "folders") ? ML_FOLDERS : ML_HOLD;
  strlcpy(out.app, p["app"] | "", sizeof(out.app));
  JsonArrayConst keys = p["keys"];
  for (int k = 0; k < MACRO_KEYS; k++) {
    JsonArrayConst e = keys[k];
    strlcpy(out.keys[k].label, e[0] | "", sizeof(out.keys[k].label));
    out.keys[k].macro = e[1] | "";
  }
  return true;
}

// ---- follow the PC's active app (companion sends "pc.app")
static char     s_pcApp[48] = "";
static uint32_t s_pcAppAt = 0;
const char* macroPcApp()    { return s_pcAppAt ? s_pcApp : ""; }
uint32_t    macroPcAppAge() { return s_pcAppAt ? millis() - s_pcAppAt : 0xFFFFFFFF; }
static bool appMatches(const char* list, const char* exe) {
  if (!list || !list[0] || !exe || !exe[0]) return false;
  const char* p = list;
  while (*p) {
    const char* e = strchr(p, ';'); size_t n = e ? (size_t)(e - p) : strlen(p);
    while (n && *p == ' ') { p++; n--; }
    while (n && p[n - 1] == ' ') n--;
    if (n && n == strlen(exe) && !strncasecmp(p, exe, n)) return true;
    if (!e) break;
    p = e + 1;
  }
  return false;
}
int macroProfileForApp(const char* exe) {
  JsonDocument doc; loadDoc(doc);
  int i = 0;
  for (JsonObjectConst p : doc["profiles"].as<JsonArrayConst>()) { if (appMatches(p["app"] | "", exe)) return i; i++; }
  return -1;
}
void macroAppChanged(const char* exe);         // AppMacro.cpp
static bool cPcApp(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  const char* exe = q["exe"] | "";
  bool changed = strcasecmp(exe, s_pcApp) != 0;
  strlcpy(s_pcApp, exe, sizeof(s_pcApp)); s_pcAppAt = millis();
  if (changed) macroAppChanged(s_pcApp);
  r["profile"] = macroProfileForApp(s_pcApp);
  return true;
}

// ---- protocol
static bool cMacroGet(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonDocument doc; loadDoc(doc);
  r["data"] = doc.as<JsonObjectConst>();
  r["stored"] = storeOk();
  r["limits"]["profiles"] = MACRO_PROFILES; r["limits"]["label"] = MACRO_LABEL_MAX; r["limits"]["name"] = MACRO_NAME_MAX;
  r["limits"]["keys"] = MACRO_KEYS; r["limits"]["app"] = MACRO_APP_MAX;
  r["pcApp"] = macroPcApp();
  return true;
}
static bool cMacroSet(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  JsonArrayConst ps = q["data"]["profiles"];
  if (ps.isNull() || ps.size() == 0 || ps.size() > MACRO_PROFILES) { r["err"] = "need 1-16 profiles"; return false; }
  if (!storeOk()) { r["err"] = "no file storage (partition scheme without SPIFFS)"; return false; }
  JsonDocument out;
  out["v"] = 2;
  JsonArray outPs = out["profiles"].to<JsonArray>();
  JsonArray errs = r["errors"].to<JsonArray>();
  int pi = 0;
  for (JsonObjectConst p : ps) {
    const char* name = p["name"] | "";
    if (!name[0] || strlen(name) > MACRO_NAME_MAX) { JsonObject e = errs.add<JsonObject>(); e["p"] = pi; e["k"] = -1; e["err"] = "name must be 1-15 chars"; }
    JsonObject op = outPs.add<JsonObject>();
    op["name"] = name;
    bool folders = !strcmp(p["layout"] | "hold", "folders");
    op["layout"] = folders ? "folders" : "hold";
    const char* app = p["app"] | "";
    if (strlen(app) > MACRO_APP_MAX) { JsonObject e = errs.add<JsonObject>(); e["p"] = pi; e["k"] = -1; e["err"] = "app list too long"; }
    op["app"] = app;
    JsonArray ok = op["keys"].to<JsonArray>();
    JsonArrayConst keys = p["keys"];
    int nk = folders ? MACRO_KEYS : MACRO_FOLDER0;
    for (int k = 0; k < nk; k++) {
      const char* lab = keys[k][0] | "";
      const char* mac = keys[k][1] | "";
      char err[48];
      if (strlen(lab) > MACRO_LABEL_MAX) { JsonObject e = errs.add<JsonObject>(); e["p"] = pi; e["k"] = k; e["err"] = "label too long"; }
      else if (!hidCheckMacro(mac, err, sizeof(err))) { JsonObject e = errs.add<JsonObject>(); e["p"] = pi; e["k"] = k; e["err"] = err; }
      JsonArray kv = ok.add<JsonArray>(); kv.add(lab); kv.add(mac);
    }
    pi++;
  }
  if (errs.size()) { r["err"] = "invalid macros"; return false; }
  String txt; serializeJson(out, txt);
  if (!storeWrite(MACRO_FILE, txt.c_str(), txt.length())) { r["err"] = "write failed"; return false; }
  cacheNames();
  return true;
}
static bool cMacroReset(JsonObjectConst, JsonObject, ProtoCtx&) { storeRemove(MACRO_FILE); cacheNames(); return true; }
static bool cMacroCheck(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  char err[48];
  if (hidCheckMacro(q["macro"] | "", err, sizeof(err))) return true;
  r["err"] = err; return false;
}
static bool cMacroRun(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  hidBleStart();
  int d = q["delay"] | 0;
  String m;
  if (d > 0) { m = "{delay "; m += String(clampv(d, 1, 10000)); m += "}"; }
  m += (const char*)(q["macro"] | "");
  char err[48];
  if (!hidRunMacro(m.c_str(), err, sizeof(err))) { r["err"] = err; return false; }
  r["route"] = hidRouteName();
  return true;
}
static bool cHidStatus(JsonObjectConst, JsonObject r, ProtoCtx&) {
  r["route"] = hidRouteName(); r["ready"] = hidReady();
  r["usb"] = hidUsbMounted(); r["ble"] = hidBleStarted(); r["bleConnected"] = hidBleConnected();
  return true;
}
static bool cHidUnpair(JsonObjectConst, JsonObject, ProtoCtx&) { hidUnpairAll(); toast("Pairings cleared"); return true; }

void macrosBegin() {
  cacheNames();
  protoRegister("macro.get",   cMacroGet);
  protoRegister("macro.set",   cMacroSet);
  protoRegister("macro.reset", cMacroReset);
  protoRegister("macro.check", cMacroCheck);
  protoRegister("macro.run",   cMacroRun);
  protoRegister("hid.status",  cHidStatus);
  protoRegister("hid.unpair",  cHidUnpair);
  protoRegister("pc.app",      cPcApp);
}
