// Focus timer (keeps running when you leave the app)
//   SELECT      start / pause          RIGHT     skip to the next phase
//   hold LEFT   reset                  hold SELECT back (timer keeps going)
// Lengths in Settings > Pomodoro. Phase changes chime, vibrate, notify and
// emit smart-home events (pomodoro/focus, pomodoro/break, pomodoro/done).
#include "Multitool.h"
#include <math.h>

void pomEnter() {}

static void ring(int cx, int cy, int r, float frac) {
  u8g2.drawCircle(cx, cy, r);
  int steps = (int)(frac * 120);
  for (int i = 0; i <= steps; i++) {
    float a = -PI / 2 + i * (2 * PI / 120);
    for (int k = 1; k <= 3; k++) u8g2.drawPixel(cx + (int)roundf(cosf(a) * (r - k)), cy + (int)roundf(sinf(a) * (r - k)));
  }
}

void pomLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnTap(BTN_SELECT)) { pomToggle(); fbSelect(); }
  if (btnTap(BTN_RIGHT))  { pomSkip(); fbNav(); }
  if (btnLong(BTN_LEFT))  { pomReset(); toast("Reset"); fbCancel(); }

  PomPhase ph = pomPhase();
  static const char* NAME[] = { "Ready", "Focus", "Short break", "Long break" };
  uiHeader("Pomodoro");
  uint32_t left = ph == POM_IDLE ? (uint32_t)S.pomWork * 60000UL : pomLeftMs();
  uint32_t total = ph == POM_IDLE ? left : pomPhaseMs();
  float frac = total ? 1.0f - (float)left / total : 0;
  ring(30, 36, 20, frac);
  uint32_t s = (left + 999) / 1000;
  char t[8]; snprintf(t, sizeof(t), "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  u8g2.setFont(F_UI); u8g2.drawStr(30 - u8g2.getStrWidth(t) / 2, 40, t);

  u8g2.setFont(F_UI); u8g2.drawStr(62, 26, NAME[ph]);
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(62, 37, ph != POM_IDLE && !pomRunning() ? "paused" : (pomRunning() ? "running" : ""));
  for (int i = 0; i < S.pomRounds; i++) {               // round dots
    int x = 64 + i * 8, y = 45;
    if (i < pomRound() - 1 || (i == pomRound() - 1 && ph != POM_FOCUS && ph != POM_IDLE)) u8g2.drawDisc(x, y, 2);
    else if (i == pomRound() - 1 && ph == POM_FOCUS) { u8g2.drawCircle(x, y, 2); u8g2.drawPixel(x, y); }
    else u8g2.drawCircle(x, y, 2);
  }
  char td[16]; snprintf(td, sizeof(td), "today %u", pomDoneToday()); u8g2.drawStr(62, 57, td);
  u8g2.setFont(F_TINY); uiCenter(63, "SEL start/pause  > skip");
  uint32_t c = ph == POM_FOCUS ? strip.Color(160, 20, 0) : (ph == POM_IDLE ? strip.Color(40, 20, 0) : strip.Color(0, 140, 40));
  npxSetBase(pomRunning() ? NPX_BREATHE : NPX_SOLID, c, 2);
}
