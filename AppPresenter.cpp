// Slide clicker + talk timer + laser pointer
//   RIGHT              next slide            LEFT / UP     previous slide
//   tap DOWN           next slide (one-thumb clicking)
//   hold DOWN          LASER POINTER: tilt to move it, let go to hide it
//                      (PowerPoint Ctrl+L / Google Slides "L" / plain mouse,
//                       Settings > Presenter > Pointer; same motion engine
//                       and calibration as the Air Mouse)
//   SELECT             start / pause the talk timer
//   hold LEFT          set the talk length (0 = just count up), resets timer
//   hold RIGHT         start the show (F5)   hold UP  black screen (B)
//   hold SELECT        back (keep holding: screen off, clicker keeps working)
// Talk timer counts DOWN from the talk length: a double buzz "Warn before end"
// minutes out, a long hard buzz + red LED at zero, then it counts the
// overtime with a short buzz every minute. The screen stays (dimmed) on
// while the timer runs.
#include "Multitool.h"

static uint32_t s_elapsed = 0, s_startedAt = 0;
static bool     s_running = false;
static int      s_slide = 1;
static int32_t  s_lastBuzzMin = -999;
static bool     s_warned = false, s_ended = false;
static bool     s_laser = false;          // laser currently shown
static uint32_t s_downAt = 0;
static int      s_editMin = 0;

static uint32_t elapsedMs() { return s_elapsed + (s_running ? millis() - s_startedAt : 0); }

static void laserOn() {
  if (s_laser) return;
  s_laser = true; pointerReset();
  if (S.presPtr == 0) hidTap(HM_LCTRL, HK_L);          // PowerPoint: Ctrl+L = laser pointer
  else if (S.presPtr == 1) hidTap(0, HK_L);            // Google Slides: L toggles the laser
  motorPulseStr(10, 3);
}
static void laserOff() {
  if (!s_laser) return;
  s_laser = false;
  if (S.presPtr == 0) hidTap(HM_LCTRL, 0x0B);          // Ctrl+H: hide the pointer again
  else if (S.presPtr == 1) hidTap(0, HK_L);
}

void presenterEnter() { hidBleStart(); pointerReset(); s_laser = false; }
void presenterExit()  { laserOff(); hidMouseButtons(0); }

static void slide(bool next) {
  static const uint8_t NEXT[3] = { HK_RIGHT, HK_PGDN, HK_SPACE };
  static const uint8_t PREV[3] = { HK_LEFT,  HK_PGUP, HK_LEFT  };
  hidTap(0, (next ? NEXT : PREV)[S.presNextKey % 3]);
  s_slide = max(1, s_slide + (next ? 1 : -1));
  if (!s_running && next && !s_elapsed) { s_running = true; s_startedAt = millis(); }   // first "next" starts the clock
  motorPulseStr(14, 5);
  npxFlash(next ? strip.Color(0, 160, 60) : strip.Color(160, 100, 0), 1, 90);
}
static void resetTimer() { s_elapsed = 0; s_running = false; s_slide = 1; s_lastBuzzMin = -999; s_warned = s_ended = false; }
static void lengthDone() { S.presAlertMin = (uint8_t)s_editMin; settingsSaveSoon(); resetTimer(); toast(s_editMin ? "Talk length set" : "Counting up"); }

void presenterLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  uint32_t now = millis();

  if (btnTap(BTN_RIGHT)) slide(true);
  if (btnTap(BTN_LEFT) || btnTap(BTN_UP)) slide(false);
  // DOWN: quick tap = next, hold = laser pointer
  if (btnPressed(BTN_DOWN)) s_downAt = now;
  if (btnDown(BTN_DOWN) && now - s_downAt > 180) laserOn();
  if (btnReleased(BTN_DOWN)) { if (s_laser) laserOff(); else slide(true); }
  if (s_laser) pointerStep(true); else pointerStep(false);

  if (btnTap(BTN_SELECT)) {
    if (s_running) { s_elapsed += now - s_startedAt; s_running = false; }
    else           { s_startedAt = now; s_running = true; }
    fbSelect();
  }
  if (btnLong(BTN_RIGHT)) { hidTap(0, HK_F5); toast("Start show"); fbConfirm(); }
  if (btnLong(BTN_UP))    { hidTap(0, HK_B);  toast("Black screen"); fbNav(); }
  if (btnLong(BTN_LEFT))  { s_editMin = S.presAlertMin; modalSlider("Talk length", &s_editMin, 0, 180, 1, " min", nullptr, lengthDone); return; }

  // ---- talk timer
  uint32_t el = elapsedMs();
  int32_t total = (int32_t)S.presAlertMin * 60000;
  int32_t left = total - (int32_t)el;                  // < 0 = overtime
  if (s_running) powerKeepAwake(1);
  if (total && s_running) {
    if (!s_warned && S.presWarnM && left <= (int32_t)S.presWarnM * 60000 && left > 0) {
      s_warned = true;
      static const uint16_t w[] = { 120, 120, 120 };
      motorPattern(w, 3); npxFlash(strip.Color(200, 120, 0), 3, 200);
    }
    if (!s_ended && left <= 0) {
      s_ended = true; s_lastBuzzMin = 0;
      static const uint16_t big[] = { 400, 150, 400, 150, 700 };
      motorPattern(big, 5); npxFlash(strip.Color(220, 0, 0), 8, 220);
      powerWake();
    }
    if (s_ended) {
      int32_t over = (-left) / 60000;
      if (over != s_lastBuzzMin) { s_lastBuzzMin = over; static const uint16_t sm[] = { 160 }; motorPattern(sm, 1); }
    }
  }

  // ---- UI
  uiHeader("Presenter");
  char t[14];
  bool over = total && left < 0;
  uint32_t shown = total ? (uint32_t)abs(left) / 1000 : el / 1000;
  if (total && !over) shown = ((uint32_t)left + 999) / 1000;
  snprintf(t, sizeof(t), "%s%02lu:%02lu", over ? "+" : "", (unsigned long)(shown / 60), (unsigned long)(shown % 60));
  u8g2.setFont(F_BIG);
  if (over && (now / 400) % 2) { int w = u8g2.getStrWidth(t); u8g2.drawBox(64 - w / 2 - 3, 16, w + 6, 28); u8g2.setDrawColor(0); }
  uiCenter(42, t);
  u8g2.setDrawColor(1);
  if (total) uiProgressBar(10, 44, 108, 5, over ? 1.0f : (float)el / total);
  u8g2.setFont(F_SMALL);
  char sl[16]; snprintf(sl, sizeof(sl), "slide %d", s_slide);
  u8g2.drawStr(2, 56, sl);
  if (s_laser) { u8g2.drawRBox(80, 49, 46, 9, 2); u8g2.setDrawColor(0); u8g2.drawStr(84, 56, "POINTER"); u8g2.setDrawColor(1); }
  else if (!s_running) uiRight(126, 56, el ? "paused" : "hold < length");
  else if (total) { char tl[12]; snprintf(tl, sizeof(tl), "of %u min", S.presAlertMin); uiRight(126, 56, tl); }
  u8g2.setFont(F_TINY);
  u8g2.drawStr(0, 63, hidReady() ? (hidRoute() == HR_USB ? "USB" : "BT") : "no PC");
  uiRight(127, 63, "SEL timer   hold v laser");
  npxSetBase(NPX_SOLID, !hidReady() ? strip.Color(90, 60, 0) : s_laser ? strip.Color(150, 0, 0) : (s_running ? strip.Color(0, 80, 20) : strip.Color(20, 20, 60)), 3);
}
