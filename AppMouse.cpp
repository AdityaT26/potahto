// Air mouse (gyro pointer) over USB or Bluetooth
//   tilt + hold DOWN   move (Settings > Air Mouse > DOWN button can invert it)
//   LEFT / RIGHT       left / right click     hold LEFT  drag (moves freely)
//   UP                 re-center              hold UP    switch USB <-> Bluetooth
//   SELECT             auto-scroll on/off (roll to scroll)
//   hold SELECT        back (keep holding: screen off, the mouse keeps working)
// The motion engine is shared with the Presenter pointer and macro hand-off
// (Pointer.cpp). Output: Settings > Bluetooth > "Keys & mouse go to".
// NOTE: no "double-tap DOWN = drag" gesture (it fired accidental clicks when
// you re-seated your hand); drag is "hold LEFT".
#include "Multitool.h"

static bool s_scroll = false;

void mouseEnter() { hidBleStart(); pointerReset(); s_scroll = false; }
void mouseExit()  { hidMouseButtons(0); settingsSaveSoon(); }

void mouseLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  bool conn = hidReady();

  if (btnTap(BTN_SELECT)) { s_scroll = !s_scroll; pointerReset(); fbSelect(); }
  if (btnTap(BTN_UP))     { pointerRecenter(); toast("Re-centered"); fbConfirm(); }
  if (btnLong(BTN_UP))    { hidToggleRoute(); toast(hidRouteName()); fbSelect(); }

  if (btnPressed(BTN_LEFT))   { if (conn) hidMousePress(HB_LEFT);  fbNav(); }
  if (btnReleased(BTN_LEFT))  { hidMouseRelease(HB_LEFT); }
  if (btnPressed(BTN_RIGHT))  { if (conn) hidMousePress(HB_RIGHT); fbNav(); }
  if (btnReleased(BTN_RIGHT)) { hidMouseRelease(HB_RIGHT); }

  bool dragging = btnDown(BTN_LEFT) && btnHeldMs(BTN_LEFT) > 200;
  bool moving = pointerClutch() || dragging;
  bool haveMpu = pointerStep(conn && moving, s_scroll);

  if (!conn)         npxSetBase(NPX_BLINK, strip.Color(0, 60, 200), 6);
  else if (!moving)  npxSetBase(NPX_SOLID, strip.Color(140, 140, 140), 5);
  else if (s_scroll) npxSetBase(NPX_SOLID, strip.Color(150, 90, 0), 5);
  else               npxSetBase(NPX_SOLID, strip.Color(0, 160, 40), 5);

  // ---- UI
  uiHeader("Air Mouse");
  // output chip: where the clicks are going
  u8g2.setFont(F_SMALL);
  const char* rt = hidRouteName();
  int cw = u8g2.getStrWidth(rt) + 8;
  if (conn) { u8g2.drawRBox(64 - cw / 2, 15, cw, 10, 3); u8g2.setDrawColor(0); }
  else u8g2.drawRFrame(64 - cw / 2, 15, cw, 10, 3);
  uiCenter(23, rt);
  u8g2.setDrawColor(1);
  u8g2.setFont(F_TINY);
  u8g2.drawStr(0, 23, "hold");
  u8g2.drawStr(0, 29, "UP");

  u8g2.setFont(F_MEDB);
  if (!haveMpu)       uiCenter(39, "no motion sensor");
  else if (dragging)  uiCenter(39, "DRAG");
  else if (s_scroll)  uiCenter(39, moving ? "SCROLLING" : "SCROLL");
  else                uiCenter(39, moving ? "MOVING" : (S.mouseHoldMoves ? "hold DOWN" : "FROZEN"));

  bool lp = btnDown(BTN_LEFT), rp = btnDown(BTN_RIGHT);
  u8g2.setFont(F_SMALL);
  if (lp) { u8g2.drawBox(26, 43, 20, 10); u8g2.setDrawColor(0); } else u8g2.drawFrame(26, 43, 20, 10);
  u8g2.drawStr(33, 51, "L"); u8g2.setDrawColor(1);
  if (rp) { u8g2.drawBox(82, 43, 20, 10); u8g2.setDrawColor(0); } else u8g2.drawFrame(82, 43, 20, 10);
  u8g2.drawStr(89, 51, "R"); u8g2.setDrawColor(1);
  u8g2.setFont(F_TINY);
  uiCenter(63, "UP center    SEL scroll");
}
