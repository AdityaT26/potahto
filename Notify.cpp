// Inbox, notification banner, find-my-device ring, clock sync
#include "Multitool.h"
#include "Proto.h"
#include <sys/time.h>
#include <time.h>

static Note     s_notes[NOTIFY_MAX];
static int      s_count = 0, s_head = 0;          // ring buffer, s_head = next write
static uint32_t s_bannerUntil = 0;
static int      s_bannerIdx = -1;

static bool     s_ring = false;
static uint32_t s_ringEnd = 0, s_ringLastBuzz = 0;
static char     s_ringTitle[20], s_ringSub[24];
static bool     s_ringSong = false;

static int slot(int i) { return (s_head - 1 - i + NOTIFY_MAX * 2) % NOTIFY_MAX; }
int         notifyCount()        { return s_count; }
const Note& notifyGet(int i)     { return s_notes[slot(i)]; }
int         notifyUnread()       { int n = 0; for (int i = 0; i < s_count; i++) if (!notifyGet(i).read) n++; return n; }
void        notifyMarkAllRead()  { for (int i = 0; i < s_count; i++) s_notes[slot(i)].read = true; }
void        notifyClear()        { s_count = 0; s_head = 0; s_bannerUntil = 0; }

void notifyPush(const char* title, const char* text, const char* src) {
  Note& n = s_notes[s_head];
  strlcpy(n.title, title && title[0] ? title : "Message", sizeof(n.title));
  strlcpy(n.text, text ? text : "", sizeof(n.text));
  strlcpy(n.src, src ? src : "", sizeof(n.src));
  n.at = millis(); n.read = false;
  s_head = (s_head + 1) % NOTIFY_MAX;
  if (s_count < NOTIFY_MAX) s_count++;
  s_bannerIdx = 0; s_bannerUntil = millis() + 4500;
  if (S.ntfMode >= 1) { static const uint16_t v[] = { 40, 80, 40 }; motorPattern(v, 3); }
  if (S.ntfMode >= 2 && !(S.buzzerEnabled && tonePlay(TONE_NOTIFY, false))) { static const ToneStep t[] = { { 1568, 60 }, { 0, 30 }, { 2093, 90 } }; buzzSeq(t, 3); }
  npxFlash(strip.Color(0, 120, 255), 3, 160);
  powerPeek(6000);                                 // light the screen briefly (lock screen shows it)
}

void ringStart(const char* title, const char* sub, uint32_t ms, uint8_t tone) {
  strlcpy(s_ringTitle, title, sizeof(s_ringTitle));
  strlcpy(s_ringSub, sub ? sub : "", sizeof(s_ringSub));
  s_ring = true; s_ringEnd = millis() + ms; s_ringLastBuzz = 0;
  s_ringSong = tonePlay((ToneKind)tone, true);
  powerWake();
}
void ringStop() { if (!s_ring) return; s_ring = false; if (s_ringSong) songStop(); s_ringSong = false; buzzAlarmStop(); motorOff(); }
bool ringActive() { return s_ring; }

void notifyLoop() {
  if (!s_ring) return;
  uint32_t now = millis();
  if ((int32_t)(now - s_ringEnd) >= 0) { ringStop(); return; }
  bool pressed = false;
  for (uint8_t b = 0; b < BTN_COUNT; b++) if (btnPressed(b)) pressed = true;
  if (pressed) { ringStop(); inputClearFrame(); fbCancel(); return; }
  if (!s_ringSong) buzzSiren(10);
  if (now - s_ringLastBuzz > 700) { s_ringLastBuzz = now; motorPulseStr(300, 9); }
  npxSetBase(NPX_BLINK, ((now / 500) & 1) ? strip.Color(255, 0, 0) : strip.Color(255, 255, 255), 8);
  npxKeepOn();
}

void notifyOverlay() {
  uint32_t now = millis();
  if (s_ring) {
    u8g2.setDrawColor(1); u8g2.drawRBox(2, 6, 124, 52, 4);
    u8g2.setDrawColor(0);
    u8g2.setFont(F_MEDB); uiCenter(26, s_ringTitle);
    u8g2.setFont(F_SMALL); uiCenter(39, s_ringSub);
    u8g2.setFont(F_TINY); uiCenter(52, "press any button");
    u8g2.setDrawColor(1);
    return;
  }
  if (s_bannerIdx < 0 || (int32_t)(now - s_bannerUntil) >= 0 || !s_count) return;
  if (currentApp() == APP_INBOX) return;          // already looking at it
  const Note& n = notifyGet(0);
  u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 128, 23); u8g2.setDrawColor(1);
  u8g2.drawRFrame(0, 0, 128, 23, 3);
  u8g2.drawBox(0, 0, 4, 23);
  u8g2.setFont(F_UI);    uiTextClipped(8, 10, n.title, 116);
  u8g2.setFont(F_SMALL); uiTextClipped(8, 20, n.text, 116);
}

// ---- protocol
static bool cNotify(JsonObjectConst q, JsonObject r, ProtoCtx& c) {
  const char* text = q["text"] | "";
  const char* title = q["title"] | "";
  if (!text[0] && !title[0]) { r["err"] = "need title or text"; return false; }
  notifyPush(title, text, q["src"] | (c.via == PV_SERIAL ? "usb" : "web"));
  return true;
}
static bool cNotifyList(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonArray a = r["notes"].to<JsonArray>();
  for (int i = 0; i < s_count; i++) {
    const Note& n = notifyGet(i);
    JsonObject o = a.add<JsonObject>();
    o["title"] = n.title; o["text"] = n.text; o["src"] = n.src; o["ago"] = (millis() - n.at) / 1000; o["read"] = n.read;
  }
  return true;
}
static bool cNotifyClear(JsonObjectConst, JsonObject, ProtoCtx&) { notifyClear(); return true; }
static bool cRing(JsonObjectConst q, JsonObject, ProtoCtx&) {
  if (q["stop"] | false) ringStop(); else ringStart("Find me!", q["text"] | "Here I am", 20000, 255);
  return true;
}
// The web app / companion sends the PC clock, so the clock works without WiFi.
// The web app / companion / phone sends its clock, so time works without WiFi.
static bool cTimeSet(JsonObjectConst q, JsonObject r, ProtoCtx& c) {
  double epoch = q["epoch"] | 0.0;
  if (epoch < 1704067200.0) { r["err"] = "bad epoch"; return false; }
  timeSetEpoch(epoch, c.via == PV_SERIAL ? "USB" : c.via == PV_BLE ? "Bluetooth" : "WiFi app");
  r["source"] = timeSource();
  return true;
}

void notifyBegin() {
  protoRegister("notify",       cNotify);
  protoRegister("notify.list",  cNotifyList);
  protoRegister("notify.clear", cNotifyClear);
  protoRegister("ring",         cRing);
  protoRegister("time.set",     cTimeSet);
}
