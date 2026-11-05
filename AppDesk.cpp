// PC Monitor, Desk Guard, Inbox
#include "Multitool.h"

// ---- PC Monitor
//  Fed by companion/potahto_companion.py (USB or WiFi). RIGHT/LEFT: page.
static uint8_t s_pcPage = 0;
void pcmonEnter() { s_pcPage = 0; }
static void bar(int y, const char* label, float pct, const char* extra) {
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(0, y + 6, label);
  if (pct < 0) { u8g2.drawStr(28, y + 6, "n/a"); return; }
  uiProgressBar(26, y, 70, 8, pct / 100.0f);
  char v[12]; snprintf(v, sizeof(v), "%d%%", (int)roundf(pct));
  u8g2.drawStr(99, y + 6, extra && extra[0] ? extra : v);
}
void pcmonLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnTap(BTN_RIGHT) || btnTap(BTN_LEFT)) { s_pcPage ^= 1; fbNav(); }
  const PcStats& p = pcStats();
  bool fresh = p.at && millis() - p.at < 6000;
  uiHeader(fresh ? p.host : "PC Monitor");
  if (!fresh) {
    u8g2.setFont(F_SMALL);
    uiCenter(26, p.at ? "PC went quiet" : "No data yet");
    u8g2.setFont(F_TINY);
    uiCenter(38, "run potahto_companion.py");
    uiCenter(46, "on your PC (folder: companion)");
    uiCenter(56, "USB or WiFi both work");
    return;
  }
  if (s_pcPage == 0) {
    bar(15, "CPU", p.cpu, "");
    bar(26, "RAM", p.ram, "");
    bar(37, "GPU", p.gpu, "");
    u8g2.setFont(F_TINY);
    char l[40];
    if (p.cpuT > 0 || p.gpuT > 0) {
      char c[12] = "", g[12] = "";
      if (p.cpuT > 0) snprintf(c, sizeof(c), "CPU %dC", (int)p.cpuT);
      if (p.gpuT > 0) snprintf(g, sizeof(g), "GPU %dC", (int)p.gpuT);
      snprintf(l, sizeof(l), "%s%s%s", c, c[0] && g[0] ? "  " : "", g);
      u8g2.drawStr(0, 54, l);
    }
    snprintf(l, sizeof(l), "down %.0f KB/s  up %.0f KB/s", max(0.0f, p.downKBs), max(0.0f, p.upKBs));
    u8g2.drawStr(0, 63, l);
  } else {
    // CPU history graph
    u8g2.setFont(F_TINY); u8g2.drawStr(0, 19, "CPU %");
    int gx = 2, gy = 21, gw = 124, gh = 40;
    u8g2.drawFrame(gx, gy, gw, gh);
    for (int i = 1; i < p.histN; i++) {
      int x0 = gx + 1 + (i - 1) * (gw - 2) / 63, x1 = gx + 1 + i * (gw - 2) / 63;
      int y0 = gy + gh - 2 - p.hist[i - 1] * (gh - 3) / 100, y1 = gy + gh - 2 - p.hist[i] * (gh - 3) / 100;
      u8g2.drawLine(x0, y0, x1, y1);
    }
    char v[8]; snprintf(v, sizeof(v), "%d%%", (int)p.cpu); uiRight(126, 19, v);
  }
  npxSetBase(NPX_SOLID, npxHSV((uint8_t)(85 - clampv<float>(p.cpu, 0, 100) * 0.85f), 255, 120), 3);
}

// ---- Desk Guard
//  The engine lives in Watch.cpp: it keeps guarding after you leave this
//  screen, with the screen off (hold SELECT) and the device locked - discreet.
//    SELECT      arm / disarm          LEFT / RIGHT  mode: seat / watch behind
//    UP / DOWN   distance threshold    hold SELECT   back (stays armed)
//  Actions are macros (web app > Desk, or Settings > Desk Guard): Win+L when
//  you leave, Shift when you're back, Alt+Tab when someone walks up, or
//  anything a macro can do ({run chrome}, {win+d}, {admin ...}).
void guardEnter() { hidBleStart(); }
void guardExit()  { settingsSaveSoon(); if (guardArmed()) toast("Still guarding"); }
void guardLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnTap(BTN_SELECT)) { guardArm(!guardArmed()); toast(guardArmed() ? "Guard on" : "Guard off"); fbSelect(); }
  if (btnTap(BTN_LEFT) || btnTap(BTN_RIGHT)) { S.dgMode ^= 1; settingsSaveSoon(); fbNav(); if (guardArmed()) { guardArm(false); guardArm(true); } }
  bool watch = S.dgMode == 1;
  if (btnRepeat(BTN_UP))   { if (watch) S.dgNearCm = clampv<int>(S.dgNearCm + 5, 20, 250); else S.dgDistCm = clampv<int>(S.dgDistCm + 5, 30, 250); settingsSaveSoon(); fbNav(); }
  if (btnRepeat(BTN_DOWN)) { if (watch) S.dgNearCm = clampv<int>(S.dgNearCm - 5, 20, 250); else S.dgDistCm = clampv<int>(S.dgDistCm - 5, 30, 250); settingsSaveSoon(); fbNav(); }
  uiHeader(watch ? "Guard: watch" : "Guard: seat");
  if (!tofPresent()) { u8g2.setFont(F_UI); uiCenter(38, "No ToF sensor"); return; }
  bool ok = watchDistanceOk(); uint16_t mm = watchDistanceMm();
  char d[12]; snprintf(d, sizeof(d), ok ? "%u" : "--", (unsigned)(mm / 10));
  uiBigValue(34, 42, d, "cm");
  GuardState st = guardState();
  static const char* NAME[] = { "OFF", "AT DESK", "leaving", "AWAY", "WATCHING", "SOMEONE!" };
  u8g2.setFont(F_SMALL);
  int bw = u8g2.getStrWidth(NAME[st]) + 6;
  bool inv = st != GS_OFF && (st != GS_INTRUDER || (millis() / 250) % 2);
  if (inv) { u8g2.drawRBox(126 - bw, 15, bw, 11, 2); u8g2.setDrawColor(0); } else u8g2.drawRFrame(126 - bw, 15, bw, 11, 2);
  u8g2.drawStr(129 - bw, 24, NAME[st]); u8g2.setDrawColor(1);
  u8g2.setFont(F_TINY);
  char l[32];
  if (watch) snprintf(l, sizeof(l), "trip < %d cm", S.dgNearCm); else snprintf(l, sizeof(l), "away > %d cm, %ds", S.dgDistCm, S.dgDelayS);
  uiRight(126, 34, l);
  if (st == GS_LEAVING) { snprintf(l, sizeof(l), "action in %lus", (unsigned long)(guardLeavingMs() / 1000 + 1)); uiRight(126, 42, l); }
  if (guardLastAction()[0]) uiTextClipped(0, 54, guardLastAction(), 127);
  u8g2.drawStr(0, 63, hidReady() ? (hidRoute() == HR_USB ? "USB" : "BT") : "no PC");
  uiRight(127, 63, "SEL arm  </> mode  U/D dist");
  if (st == GS_OFF) npxSetBase(NPX_SOLID, strip.Color(20, 20, 20), 3);
  else npxSetBase(NPX_SOLID, st == GS_AWAY || st == GS_INTRUDER ? strip.Color(60, 0, 0) : strip.Color(0, 60, 0), 3);
}

// ---- Inbox
static int  s_inSel = 0, s_inTop = 0;
static bool s_inOpen = false;
void inboxEnter() { s_inSel = 0; s_inTop = 0; s_inOpen = false; }
void inboxExit()  { notifyMarkAllRead(); }
static void ago(uint32_t at, char* o, size_t n) {
  uint32_t s = (millis() - at) / 1000;
  if (s < 60) snprintf(o, n, "%lus", (unsigned long)s);
  else if (s < 3600) snprintf(o, n, "%lum", (unsigned long)(s / 60));
  else snprintf(o, n, "%luh", (unsigned long)(s / 3600));
}
void inboxLoop() {
  int n = notifyCount();
  if (s_inOpen) {
    if (wantBack() || btnTap(BTN_LEFT) || btnTap(BTN_SELECT)) { s_inOpen = false; fbBack(); return; }
    const Note& m = notifyGet(s_inSel);
    uiHeader(m.title);
    u8g2.setFont(F_SMALL);
    // word-wrap into up to 5 lines of ~24 chars
    const char* p = m.text; int y = 22;
    while (*p && y <= 62) {
      char line[26]; int len = 0, lastSp = -1;
      while (p[len] && len < 24) { if (p[len] == ' ') lastSp = len; len++; }
      if (p[len] && lastSp > 0) len = lastSp;
      memcpy(line, p, len); line[len] = 0;
      u8g2.drawStr(2, y, line); y += 9;
      p += len; while (*p == ' ') p++;
    }
    return;
  }
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_inSel < n - 1) { s_inSel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_inSel > 0) { s_inSel--; fbNav(); } else fbEdge(); }
  if ((btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) && n) { s_inOpen = true; fbSelect(); return; }
  if (btnLong(BTN_LEFT) && n) { notifyClear(); toast("Cleared"); fbCancel(); return; }
  uiHeader("Inbox");
  if (!n) { u8g2.setFont(F_SMALL); uiCenter(34, "No notifications"); u8g2.setFont(F_TINY); uiCenter(46, "from the web app, Home Assistant"); uiCenter(54, "or the PC companion"); return; }
  int rows = uiListRows();
  if (s_inSel < s_inTop) s_inTop = s_inSel;
  if (s_inSel >= s_inTop + rows) s_inTop = s_inSel - rows + 1;
  for (int r = 0; r < rows; r++) {
    int i = s_inTop + r; if (i >= n) break;
    const Note& m = notifyGet(i);
    char a[8]; ago(m.at, a, sizeof(a));
    char lab[28]; snprintf(lab, sizeof(lab), "%s%s", m.read ? "" : "\xB7 ", m.title);
    uiListRow(r, lab, a, i == s_inSel);
  }
  uiScrollbar(n, s_inTop, rows);
}
