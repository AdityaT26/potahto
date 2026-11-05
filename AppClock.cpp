// Clock, countdown timer, alarms
//
// LEFT/RIGHT switch pages: Clock / Timer / Alarms. Hold SELECT = back.
// Timer : UP/DOWN set minutes, SELECT start/stop (keeps running in background)
// Alarms: UP/DOWN pick, SELECT on/off, hold RIGHT edit the time.
//         Repeat days are set in the web app (default Mon-Fri).
// Time comes from WiFi (NTP) or from the web app / companion over USB.
// While open, WiFi is requested so the clock can sync.
#include "Multitool.h"
#include <time.h>

static uint8_t s_page = 0, s_alSel = 0;
static int     s_editH, s_editM;
static bool    s_held = false;
static const char* DAYS = "MTWTFSS";

void clockEnter() { s_page = 0; if (S.wifiMode != WM_OFF) { netAcquire(); s_held = true; } }
void clockExit()  { if (s_held) netRelease(); s_held = false; }

static void editDoneM() { S.alarmH[s_alSel] = (uint8_t)s_editH; S.alarmM[s_alSel] = (uint8_t)s_editM; S.alarmOn[s_alSel] = true; settingsSaveSoon(); toast("Alarm set"); }
static void editDoneH() { modalSlider("Minute", &s_editM, 0, 59, 1, "", nullptr, editDoneM); }

static void drawClock() {
  uiHeader("Clock");
  if (!timeValid()) {
    u8g2.setFont(F_BIG); uiCenter(44, "--:--");
    u8g2.setFont(F_TINY); uiCenter(56, "waiting for time: WiFi or web app");
    return;
  }
  time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
  char hm[8]; strftime(hm, sizeof(hm), "%H:%M", &lt);
  u8g2.setFont(F_HUGE); uiCenter(46, hm);
  char sec[4]; snprintf(sec, sizeof(sec), "%02d", lt.tm_sec);
  u8g2.setFont(F_SMALL); u8g2.drawStr(116, 46, sec);
  char d[24]; strftime(d, sizeof(d), "%a %d %b", &lt);
  u8g2.setFont(F_SMALL); u8g2.drawStr(2, 57, d);
  int until; int n = alarmNextIndex(&until);
  if (n >= 0) {
    char a[16]; snprintf(a, sizeof(a), "alarm %02d:%02d", S.alarmH[n], S.alarmM[n]);
    uiRight(126, 57, a);
  }
  if (timerRunning()) { u8g2.setFont(F_TINY); uint32_t s = timerLeftMs() / 1000; char tm[16]; snprintf(tm, sizeof(tm), "timer %lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60)); u8g2.drawStr(2, 63, tm); }
}

static void drawTimer() {
  if (btnRepeat(BTN_UP))   { if (!timerRunning()) { S.timerMin = clampv<int>(S.timerMin + (S.timerMin >= 10 ? 5 : 1), 1, 180); settingsSaveSoon(); fbNav(); } }
  if (btnRepeat(BTN_DOWN)) { if (!timerRunning()) { S.timerMin = clampv<int>(S.timerMin - (S.timerMin > 10 ? 5 : 1), 1, 180); settingsSaveSoon(); fbNav(); } }
  if (btnTap(BTN_SELECT))  { if (timerRunning()) { timerStop(); fbCancel(); } else { timerStart(S.timerMin); fbConfirm(); } }
  uiHeader("Timer");
  char t[12];
  if (timerRunning()) { uint32_t s = (timerLeftMs() + 999) / 1000; snprintf(t, sizeof(t), "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60)); }
  else snprintf(t, sizeof(t), "%u:00", S.timerMin);
  u8g2.setFont(F_BIG); uiCenter(44, t);
  if (timerRunning()) uiProgressBar(14, 49, 100, 6, 1.0f - timerLeftMs() / (S.timerMin * 60000.0f));
  u8g2.setFont(F_TINY); uiCenter(63, timerRunning() ? "SEL stop" : "U/D minutes  SEL start");
}

static void drawAlarms() {
  if (btnRepeat(BTN_DOWN)) { s_alSel = (s_alSel + 1) % ALARMS; fbNav(); }
  if (btnRepeat(BTN_UP))   { s_alSel = (s_alSel + ALARMS - 1) % ALARMS; fbNav(); }
  if (btnTap(BTN_SELECT))  { S.alarmOn[s_alSel] = !S.alarmOn[s_alSel]; settingsSaveSoon(); fbSelect(); }
  if (btnLong(BTN_RIGHT))  { s_editH = S.alarmH[s_alSel]; s_editM = S.alarmM[s_alSel]; modalSlider("Hour", &s_editH, 0, 23, 1, "", nullptr, editDoneH); }
  uiHeader("Alarms");
  for (int i = 0; i < ALARMS; i++) {
    int top = BODY_Y + i * 15 + 1;
    bool sel = i == s_alSel;
    if (sel) { u8g2.drawRBox(0, top, 128, 14, 2); u8g2.setDrawColor(0); }
    u8g2.setFont(F_UI);
    char t[8]; snprintf(t, sizeof(t), "%02d:%02d", S.alarmH[i], S.alarmM[i]); u8g2.drawStr(4, top + 11, t);
    u8g2.setFont(F_TINY);
    if (S.alarmDays[i] == 0) u8g2.drawStr(42, top + 10, "once");
    else for (int d = 0; d < 7; d++) {
      char c[2] = { DAYS[d], 0 };
      if ((S.alarmDays[i] >> d) & 1) u8g2.drawStr(42 + d * 6, top + 10, c); else u8g2.drawPixel(43 + d * 6, top + 8);
    }
    u8g2.setFont(F_SMALL);
    u8g2.drawStr(100, top + 10, S.alarmOn[i] ? "ON" : "off");
    if (sel) u8g2.setDrawColor(1);
  }
  u8g2.setFont(F_TINY); uiCenter(63, "SEL on/off   hold > edit time");
}

void clockLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnTap(BTN_RIGHT)) { s_page = (s_page + 1) % 3; fbNav(); }
  if (btnTap(BTN_LEFT))  { s_page = (s_page + 2) % 3; fbNav(); }
  if (s_page == 0) drawClock(); else if (s_page == 1) drawTimer(); else drawAlarms();
  npxSetBase(NPX_BREATHE, strip.Color(20, 20, 60), 2);
}
