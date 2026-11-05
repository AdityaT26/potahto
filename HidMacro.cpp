// Macro parser + non-blocking playback queue
//
// Transport-independent: it only calls the primitives in HidPriv.h, so it is
// unit-tested on the PC (_dev/host_check/tests/test_macro.cpp).
#include "Multitool.h"
#include "HidPriv.h"

// ---- ASCII -> HID usage (US layout)
// 0x20..0x7E: low 7 bits usage, bit 7 = needs shift
static const uint8_t ASCII_HID[95] = {
  0x2C, 0x9E, 0xB4, 0xA0, 0xA1, 0xA2, 0xA4, 0x34, 0xA6, 0xA7, 0xA5, 0xAE, 0x36, 0x2D, 0x37, 0x38,   //  !"#$%&'()*+,-./
  0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0xB3, 0x33, 0xB6, 0x2E, 0xB7, 0xB8,   // 0-9 :;<=>?
  0x9F, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92,   // @A-O
  0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x2F, 0x31, 0x30, 0xA3, 0xAD,   // P-Z [\]^_
  0x35, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,   // `a-o
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0xAF, 0xB1, 0xB0, 0xB5,         // p-z {|}~
};
static bool asciiUsage(char c, uint8_t& mods, uint8_t& usage) {
  if (c == '\n') { mods = 0; usage = HK_ENTER; return true; }
  if (c == '\t') { mods = 0; usage = HK_TAB; return true; }
  if (c < 0x20 || c > 0x7E) return false;
  uint8_t v = ASCII_HID[c - 0x20];
  mods = (v & 0x80) ? HM_LSHIFT : 0; usage = v & 0x7F;
  return true;
}

// ---- playback queue
enum : uint8_t { QA_TAP, QA_CONS, QA_DELAY, QA_CLICK, QA_SCROLL, QA_POINTER, QA_EVENT };
// text arguments of {pc ...} live here (the queue itself only holds numbers)
#define PCQ 4
static char    s_pcArg[PCQ][200];
static uint8_t s_pcNext = 0;
struct QAct { uint8_t type, mods; int16_t code; };
#define QLEN 640
static QAct     s_q[QLEN];
static uint16_t s_qHead = 0, s_qTail = 0;
static uint8_t  s_phase = 0;
static uint32_t s_nextAt = 0, s_waitSince = 0;

static uint16_t qCount() { return (uint16_t)((s_qTail + QLEN - s_qHead) % QLEN); }
static bool qPush(uint8_t t, uint8_t m, int16_t c) {
  if (qCount() >= QLEN - 1) return false;
  s_q[s_qTail] = { t, m, c }; s_qTail = (s_qTail + 1) % QLEN; return true;
}
bool hidBusy() { return qCount() > 0; }
void hidCancel() { s_qHead = s_qTail = 0; s_phase = 0; hidReleaseAll(); }

void hidTap(uint8_t mods, uint8_t usage) { qPush(QA_TAP, mods, usage); }
void hidConsumerTap(uint16_t usage)      { qPush(QA_CONS, 0, (int16_t)usage); }
void hidType(const char* t) {
  for (; *t; t++) { uint8_t m, u; if (asciiUsage(*t, m, u)) qPush(QA_TAP, m, u); }
}

// ---- macro parser
struct Name16 { const char* n; uint16_t v; };
static const Name16 KEYS[] = {
  {"enter",0x28},{"return",0x28},{"esc",0x29},{"escape",0x29},{"backspace",0x2A},{"bksp",0x2A},
  {"tab",0x2B},{"space",0x2C},{"capslock",0x39},{"printscreen",0x46},{"prtsc",0x46},{"scrolllock",0x47},
  {"pause",0x48},{"insert",0x49},{"ins",0x49},{"home",0x4A},{"pgup",0x4B},{"pageup",0x4B},
  {"delete",0x4C},{"del",0x4C},{"end",0x4D},{"pgdn",0x4E},{"pagedown",0x4E},{"right",0x4F},
  {"left",0x50},{"down",0x51},{"up",0x52},{"numlock",0x53},{"menu",0x65},{"app",0x65},
  {"minus",0x2D},{"equal",0x2E},{"plus",0x2E},{"comma",0x36},{"period",0x37},{"slash",0x38},
};
static const Name16 MODS[] = {
  {"ctrl",HM_LCTRL},{"control",HM_LCTRL},{"shift",HM_LSHIFT},{"alt",HM_LALT},{"option",HM_LALT},
  {"win",HM_LGUI},{"gui",HM_LGUI},{"cmd",HM_LGUI},{"super",HM_LGUI},{"meta",HM_LGUI},
  {"rctrl",HM_RCTRL},{"rshift",HM_RSHIFT},{"ralt",HM_RALT},{"altgr",HM_RALT},{"rwin",HM_RGUI},
};
static const Name16 MEDIA[] = {
  {"playpause",0xCD},{"play",0xCD},{"pause",0xCD},{"next",0xB5},{"prev",0xB6},{"previous",0xB6},
  {"stop",0xB7},{"mute",0xE2},{"volup",0xE9},{"voldown",0xEA},{"brightup",0x6F},{"brightdown",0x70},
  {"calc",0x192},{"mail",0x18A},{"browser",0x196},{"search",0x221},{"home",0x223},{"back",0x224},
  {"forward",0x225},{"refresh",0x227},{"files",0x194},
};
template <size_t N> static bool lookup(const Name16 (&tbl)[N], const char* s, uint16_t& v) {
  for (size_t i = 0; i < N; i++) if (!strcmp(tbl[i].n, s)) { v = tbl[i].v; return true; }
  return false;
}
static bool keyUsage(const char* k, uint8_t& usage) {
  uint16_t v;
  if (lookup(KEYS, k, v)) { usage = (uint8_t)v; return true; }
  if (k[0] == 'f' && isdigit((unsigned char)k[1])) {          // f1..f24
    int n = atoi(k + 1);
    if (n >= 1 && n <= 12) { usage = 0x3A + n - 1; return true; }
    if (n >= 13 && n <= 24) { usage = 0x68 + n - 13; return true; }
  }
  if (k[0] && !k[1]) {                                         // single character
    uint8_t m; return asciiUsage(k[0], m, usage);              // (shift implied by the char is ignored in chords)
  }
  return false;
}
static void setErr(char* err, size_t n, const char* fmt, const char* tok) { if (err && n) snprintf(err, n, fmt, tok); }

static bool typeRaw(const char* t, bool queue) {
  for (; *t; t++) { uint8_t m, u; if (asciiUsage(*t, m, u)) { if (queue && !qPush(QA_TAP, m, u)) return false; } }
  return true;
}
// open the Run box (Win+R) and wait for it
static bool runBox(bool queue) { return !queue || (qPush(QA_TAP, HM_LGUI, 0x15) && qPush(QA_DELAY, 0, 450)); }

// raw: contents of {...}; the first word is the command (case-insensitive),
// the rest is kept as typed (commands, paths and URLs care about case)
static bool parseToken(const char* raw, bool queue, char* err, size_t errLen) {
  char name[24]; size_t i = 0;
  while (raw[i] && raw[i] != ' ' && i < sizeof(name) - 1) { name[i] = (char)tolower((unsigned char)raw[i]); i++; }
  name[i] = 0;
  const char* rawArg = raw + i; while (*rawArg == ' ') rawArg++;
  // ---- text commands
  if (!strcmp(name, "run") || !strcmp(name, "cmd") || !strcmp(name, "admin") || !strcmp(name, "pc")) {
    if (!*rawArg) { setErr(err, errLen, "{%s} needs something to run", name); return false; }
    if (strlen(rawArg) > 190) { setErr(err, errLen, "{%s ...} is too long (max 190)", name); return false; }
    if (!strcmp(name, "pc")) {
      if (!queue) return true;
      uint8_t k = s_pcNext; s_pcNext = (s_pcNext + 1) % PCQ;
      strlcpy(s_pcArg[k], rawArg, sizeof(s_pcArg[k]));
      return qPush(QA_EVENT, 0, k);
    }
    if (!runBox(queue)) return false;
    if (!strcmp(name, "run")) return typeRaw(rawArg, queue) && (!queue || qPush(QA_TAP, 0, HK_ENTER));
    if (!typeRaw("cmd /k ", queue) || !typeRaw(rawArg, queue)) return false;
    if (!strcmp(name, "cmd")) return !queue || qPush(QA_TAP, 0, HK_ENTER);
    // admin: Ctrl+Shift+Enter in the Run box = "run as administrator", then
    // accept the UAC prompt with Alt+Y (keyboard input from a USB/Bluetooth
    // HID device is real input, so the secure desktop takes it)
    return !queue || (qPush(QA_TAP, HM_LCTRL | HM_LSHIFT, HK_ENTER) && qPush(QA_DELAY, 0, 1800) && qPush(QA_TAP, HM_LALT, 0x1C));
  }
  if (!strcmp(name, "pointer")) {
    char a[12]; strlcpy(a, rawArg, sizeof(a)); for (char* p = a; *p; p++) *p = (char)tolower((unsigned char)*p);
    uint8_t mode = !a[0] || !strcmp(a, "free") ? PTR_FREE : !strcmp(a, "click") ? PTR_CLICK : !strcmp(a, "drag") ? PTR_DRAG : 255;
    if (mode == 255) { setErr(err, errLen, "pointer mode must be click, drag or empty", ""); return false; }
    return !queue || qPush(QA_POINTER, 0, mode);
  }
  // everything else is case-insensitive
  char tok[40];
  size_t n = strlen(raw);
  if (n >= sizeof(tok)) { setErr(err, errLen, "bad token", ""); return false; }
  for (size_t k = 0; k <= n; k++) tok[k] = (char)tolower((unsigned char)raw[k]);
  char* arg = strchr(tok, ' ');
  if (arg) { *arg++ = 0; while (*arg == ' ') arg++; }
  uint16_t v;
  if (!strcmp(tok, "delay") || !strcmp(tok, "wait")) {
    int ms = arg ? atoi(arg) : 0;
    if (ms <= 0 || ms > 30000) { setErr(err, errLen, "delay must be 1..30000 ms", ""); return false; }
    return !queue || qPush(QA_DELAY, 0, (int16_t)ms);
  }
  if (!strcmp(tok, "media")) {
    if (!arg || !lookup(MEDIA, arg, v)) { setErr(err, errLen, "unknown media key '%s'", arg ? arg : ""); return false; }
    return !queue || qPush(QA_CONS, 0, (int16_t)v);
  }
  if (!strcmp(tok, "mouse") || !strcmp(tok, "click")) {
    uint8_t b = (!arg || !strcmp(arg, "left")) ? HB_LEFT : !strcmp(arg, "right") ? HB_RIGHT : !strcmp(arg, "middle") ? HB_MIDDLE : 0;
    if (!b) { setErr(err, errLen, "mouse button must be left/right/middle", ""); return false; }
    return !queue || qPush(QA_CLICK, 0, b);
  }
  if (!strcmp(tok, "scroll")) {
    int n = arg ? atoi(arg) : 0;
    if (!n || n < -100 || n > 100) { setErr(err, errLen, "scroll needs -100..100", ""); return false; }
    return !queue || qPush(QA_SCROLL, 0, (int16_t)n);
  }
  // chord: mod+mod+key  (or a single key / single modifier)
  uint8_t mods = 0, usage = 0;
  char* p = tok;
  while (p && *p) {
    char* plus = strchr(p, '+');
    if (plus == p) { plus = strchr(p + 1, '+'); }             // allow "{ctrl++}"
    if (plus) *plus = 0;
    if (lookup(MODS, p, v)) mods |= (uint8_t)v;
    else if (!usage && keyUsage(p, usage)) {}
    else { setErr(err, errLen, "unknown key '%s'", p); return false; }
    p = plus ? plus + 1 : nullptr;
  }
  if (!mods && !usage) { setErr(err, errLen, "empty {}", ""); return false; }
  return !queue || qPush(QA_TAP, mods, usage);
}

static bool parseMacro(const char* m, bool queue, char* err, size_t errLen) {
  if (!m) return true;
  if (strlen(m) > 600) { setErr(err, errLen, "macro too long (max 600)", ""); return false; }
  uint16_t startTail = s_qTail;
  for (const char* p = m; *p; ) {
    if (p[0] == '{' && p[1] == '{') { if (queue && !qPush(QA_TAP, HM_LSHIFT, 0x2F)) goto full; p += 2; continue; }
    if (*p == '{') {
      const char* end = strchr(p, '}');
      if (!end) { setErr(err, errLen, "missing '}'", ""); goto fail; }
      char tok[220]; size_t n = end - p - 1;
      if (n == 0 || n >= sizeof(tok)) { setErr(err, errLen, "bad token", ""); goto fail; }
      memcpy(tok, p + 1, n); tok[n] = 0;
      while (n && tok[n - 1] == ' ') tok[--n] = 0;
      { size_t lead = 0; while (tok[lead] == ' ') lead++; if (lead) memmove(tok, tok + lead, n - lead + 1); }
      if (!parseToken(tok, queue, err, errLen)) { if (err && !err[0]) setErr(err, errLen, "queue full", ""); goto fail; }
      p = end + 1; continue;
    }
    uint8_t mm, u;
    if (asciiUsage(*p, mm, u)) { if (queue && !qPush(QA_TAP, mm, u)) goto full; }
    p++;                                                       // non-ASCII bytes are skipped
  }
  return true;
full:
  setErr(err, errLen, "macro queue full", "");
fail:
  if (queue) s_qTail = startTail;                              // roll back a half-queued macro
  return false;
}
bool hidCheckMacro(const char* m, char* err, size_t n) { if (err && n) err[0] = 0; return parseMacro(m, false, err, n); }
bool hidRunMacro(const char* m, char* err, size_t n) {
  if (err && n) err[0] = 0;
  if (!parseMacro(m, false, err, n)) return false;             // validate fully before queueing
  return parseMacro(m, true, err, n);
}

void hidMacroTick() {
  uint32_t now = millis();
  if (!qCount() || (int32_t)(now - s_nextAt) < 0) return;
  uint8_t headType = s_q[s_qHead].type;
  if (headType != QA_EVENT && headType != QA_DELAY && !hidReady()) {   // wait up to 5 s for a host
    if (!s_waitSince) s_waitSince = now;
    if (now - s_waitSince > 5000) { hidCancel(); s_waitSince = 0; }
    return;
  }
  s_waitSince = 0;
  const uint16_t kd = hidxIsUsb() ? 8 : 16;              // per-report gap
  QAct& a = s_q[s_qHead];
  bool done = false;
  switch (a.type) {
    case QA_TAP:
      if (s_phase == 0) { hidxKbd(a.mods, (uint8_t)a.code); s_phase = 1; }
      else              { hidxKbd(0, 0); done = true; }
      s_nextAt = now + kd; break;
    case QA_CONS:
      if (s_phase == 0) { hidxCons((uint16_t)a.code); s_phase = 1; s_nextAt = now + 30; }
      else              { hidxCons(0); done = true; s_nextAt = now + kd; }
      break;
    case QA_DELAY:
      if (s_phase == 0) { s_phase = 1; s_nextAt = now + a.code; }
      else done = true;
      break;
    case QA_CLICK:
      if (s_phase == 0) { hidMouseButtons(hidxButtons() | (uint8_t)a.code); s_phase = 1; s_nextAt = now + 20; }
      else              { hidMouseButtons(hidxButtons() & ~(uint8_t)a.code); done = true; s_nextAt = now + kd; }
      break;
    case QA_SCROLL:
      hidMouseMove(0, 0, a.code, 0); done = true; s_nextAt = now + kd; break;
    case QA_POINTER:
      hidxPointer((uint8_t)a.code); done = true; s_nextAt = now + kd; break;
    case QA_EVENT:
      hidxEvent("pc.run", s_pcArg[a.code % PCQ]); done = true; s_nextAt = now; break;
  }
  if (done) { s_phase = 0; s_qHead = (s_qHead + 1) % QLEN; }
}
