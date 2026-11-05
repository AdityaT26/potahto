// ToF break-beam alarm (motor / buzzer / LED mix)
#include "Multitool.h"

// The alarm itself runs in Watch.cpp, so it keeps guarding after you leave
// this screen, with the screen off and the device locked.
static int s_sel = 0;          // 0 thr, 1 mot, 2 buz, 3 led, 4 vol

void tripEnter() {}
void tripExit()  { settingsSaveSoon(); if (tripArmed()) toast("Still armed"); }

static void changeSel(int dir) {
  switch (s_sel) {
    case 0: S.tripDistMm = clampv<int>(S.tripDistMm + dir * 10, 30, 2000); break;
    case 1: S.tripUseMotor  = !S.tripUseMotor;  break;
    case 2: S.tripUseBuzzer = !S.tripUseBuzzer; break;
    case 3: S.tripUseLed    = !S.tripUseLed;    break;
    case 4: S.tripVolume = clampv<int>(S.tripVolume + dir, 0, 10); break;
  }
  fbNav(); settingsSaveSoon();
}

void tripLoop() {
  if (!tofPresent()) {
    uiHeader("Tripwire");
    u8g2.setFont(F_UI); uiCenter(38, "No ToF sensor");
    if (wantBack()) { fbBack(); goBack(); }
    return;
  }
  if (wantBack()) { fbBack(); goBack(); return; }

  if (btnRepeat(BTN_DOWN)) { s_sel = (s_sel + 1) % 5; fbNav(); }
  if (btnRepeat(BTN_UP))   { s_sel = (s_sel + 4) % 5; fbNav(); }
  if (btnRepeat(BTN_RIGHT)) changeSel(+1);
  if (btnRepeat(BTN_LEFT))  changeSel(-1);
  if (btnTap(BTN_SELECT)) {
    tripArm(!tripArmed());
    if (tripArmed()) { fbConfirm(); toast("Armed"); } else { fbCancel(); toast("Disarmed"); }
  }
  bool ok = watchDistanceOk(); uint16_t mm = watchDistanceMm();
  bool s_tripped = tripTripped(), s_armed = tripArmed();
  if (!s_tripped) {
    if (s_armed) { if (S.tripUseLed) npxSetBase(NPX_BREATHE, strip.Color(0, 150, 30), 3); else npxOff(); }
    else npxSetBase(NPX_SOLID, strip.Color(30, 30, 30), 3);
  }

  // ---- UI
  uiHeader("Tripwire");

  // row A: live distance + state
  u8g2.setFont(F_SMALL);
  char d[16];
  if (ok) snprintf(d, sizeof(d), "%d mm", mm); else snprintf(d, sizeof(d), "-- mm");
  u8g2.drawStr(4, 24, d);
  const char* badge = s_tripped ? "TRIPPED" : (s_armed ? "ARMED" : "SAFE");
  int bw = u8g2.getStrWidth(badge) + 6;
  if (s_tripped || s_armed) {
    if (!s_tripped || (millis() / 250) % 2) {          // flash when tripped
      u8g2.drawRBox(126 - bw, 15, bw, 11, 2);
      u8g2.setDrawColor(0); u8g2.drawStr(126 - bw + 3, 24, badge); u8g2.setDrawColor(1);
    }
  } else {
    u8g2.drawStr(126 - u8g2.getStrWidth(badge), 24, badge);
  }
  u8g2.setFont(F_TINY);
  u8g2.drawStr(52, 23, S.tripAutoReset ? "auto" : "latch");

  // row B: threshold
  if (s_sel == 0) { u8g2.drawRBox(0, 28, 128, 12, 2); u8g2.setDrawColor(0); }
  u8g2.setFont(F_UI);
  u8g2.drawStr(4, 37, "Trip <");
  char thr[10]; snprintf(thr, sizeof(thr), "%d mm", S.tripDistMm);
  u8g2.drawStr(122 - u8g2.getStrWidth(thr), 37, thr);
  if (s_sel == 0) u8g2.setDrawColor(1);

  // row C: output toggles
  const char* names[3] = { "Mot", "Buz", "LED" };
  bool states[3] = { S.tripUseMotor, S.tripUseBuzzer, S.tripUseLed };
  for (int i = 0; i < 3; i++) {
    int tx = 2 + i * 42;
    bool tsel = (s_sel == 1 + i);
    if (tsel) { u8g2.drawRBox(tx, 40, 40, 12, 2); u8g2.setDrawColor(0); }
    u8g2.setFont(F_SMALL);
    u8g2.drawStr(tx + 3, 49, names[i]);
    if (states[i]) u8g2.drawDisc(tx + 30, 45, 3); else u8g2.drawCircle(tx + 30, 45, 3);
    if (tsel) u8g2.setDrawColor(1);
  }

  // row D: volume
  if (s_sel == 4) { u8g2.drawRBox(0, 52, 128, 12, 2); u8g2.setDrawColor(0); }
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(4, 61, "Vol");
  uiProgressBar(28, 54, 70, 7, S.tripVolume / 10.0f);
  char vv[6]; snprintf(vv, sizeof(vv), "%d", S.tripVolume);
  u8g2.drawStr(122 - u8g2.getStrWidth(vv), 61, vv);
  if (s_sel == 4) u8g2.setDrawColor(1);
}
