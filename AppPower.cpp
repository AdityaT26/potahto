// System > Power: screen off, lock, deep sleep, restart
//
// Shortcut for the same things the power gesture does (keep holding SELECT
// past "back" = screen off), plus deep sleep and the power settings.
#include "Multitool.h"

static int s_sel = 0, s_top = 0;
enum { P_OFF, P_LOCK, P_SLEEP, P_RESTART, P_SETTINGS, P_N };
static const char* const NAME[P_N] = { "Screen off", "Lock now", "Deep sleep", "Restart", "Power settings" };

void powerAppEnter() { s_sel = 0; s_top = 0; }
void powerAppLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_sel < P_N - 1) { s_sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_sel > 0) { s_sel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) {
    fbSelect();
    switch (s_sel) {
      case P_OFF:      powerScreenOff(false); break;
      case P_LOCK:     powerLockNow(); break;
      case P_SLEEP:    modalConfirm("Deep sleep", "UP/DOWN/LEFT wakes it", []() { powerDeepSleep(); }); break;
      case P_RESTART:  modalConfirm("Restart", "Restart now?", []() { powerRestart(); }); break;
      case P_SETTINGS: settingsOpenCategory("Power & lock"); break;
    }
    return;
  }
  uiHeader("Power");
  // battery strip
  u8g2.setFont(F_SMALL);
  char b[32]; snprintf(b, sizeof(b), "%u%%  %.2fV%s", battPercent(), battVoltage(), battCharging() ? "  charging" : "");
  u8g2.drawStr(2, 21, b);
  u8g2.drawHLine(0, 24, 128);
  const int rows = 3;
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + rows) s_top = s_sel - rows + 1;
  for (int r = 0; r < rows; r++) {
    int i = s_top + r; if (i >= P_N) break;
    int top = 26 + r * 12; bool sel = i == s_sel;
    if (sel) { u8g2.drawRBox(0, top, 125, 12, 2); u8g2.setDrawColor(0); }
    u8g2.setFont(F_UI); u8g2.drawStr(5, top + 10, NAME[i]);
    if (i == P_OFF) { u8g2.setFont(F_TINY); uiRight(122, top + 9, "or hold SEL"); }
    u8g2.setDrawColor(1);
  }
  uiScrollbar(P_N, s_top, rows);
}
