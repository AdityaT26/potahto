// Bluetooth camera remote for phones
//
// Pair your phone with "Potahto" (Bluetooth settings), open the camera app.
// Phones take a photo on "volume up", which is what this sends.
//   SELECT      shoot now               RIGHT     self-timer (Settings: seconds)
//   DOWN        burst of 5              UP        wave trigger on/off: wave a
//                                                 hand over the ToF sensor to shoot
//   hold SELECT back
#include "Multitool.h"

static uint32_t s_countdownEnd = 0;
static bool     s_wave = false, s_handIn = false;
static uint32_t s_lastShot = 0, s_lastBeep = 0;
static int      s_shots = 0;

void shutterEnter() { hidBleStart(); hidRouteOverride(HR_BLE); s_countdownEnd = 0; s_wave = false; }   // phones: always Bluetooth
void shutterExit()  { hidCancel(); hidRouteOverride(HR_AUTO); }

static void shoot() {
  hidConsumerTap(HC_VOLUP);
  s_shots++; s_lastShot = millis();
  static const ToneStep clk[] = { { 2800, 15 }, { 0, 25 }, { 1800, 25 } };
  buzzSeq(clk, 3);
  motorPulseStr(25, 6);
  npxFlash(strip.Color(255, 255, 255), 1, 120);
}

void shutterLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  uint32_t now = millis();
  bool ready = hidReady();

  if (btnTap(BTN_SELECT)) { if (ready) shoot(); else fbError(); }
  if (btnTap(BTN_RIGHT)) {
    if (s_countdownEnd) { s_countdownEnd = 0; fbCancel(); }
    else { s_countdownEnd = now + max<uint32_t>(1, S.shutterDelay) * 1000UL; s_lastBeep = 0; fbSelect(); }
  }
  if (btnTap(BTN_DOWN) && ready) {
    for (int i = 0; i < 5; i++) { hidConsumerTap(HC_VOLUP); hidRunMacro("{delay 350}"); }
    s_shots += 5; fbConfirm();
  }
  if (btnTap(BTN_UP)) {
    if (!tofPresent()) { toast("No ToF sensor"); fbError(); }
    else { s_wave = !s_wave; toast(s_wave ? "Wave to shoot" : "Wave off"); fbSelect(); }
  }

  // self-timer: beep each second, fast beeps in the last one
  if (s_countdownEnd) {
    int32_t left = (int32_t)(s_countdownEnd - now);
    if (left <= 0) { s_countdownEnd = 0; if (hidReady()) shoot(); else fbError(); }
    else {
      uint32_t period = left < 1000 ? 200 : 1000;
      if (now - s_lastBeep >= period) { s_lastBeep = now; buzzTone(left < 1000 ? 2600 : 2000, 40); npxFlash(strip.Color(200, 0, 0), 1, 80); }
    }
  }

  // wave trigger: hand enters < 15 cm, with a 2 s re-arm
  if (s_wave) {
    uint16_t mm;
    bool in = tofReadMm(mm) && mm < 150;
    if (in && !s_handIn && now - s_lastShot > 2000 && ready) shoot();
    s_handIn = in;
  }

  // ---- UI
  uiHeader("Shutter");
  if (s_countdownEnd) {
    char c[4]; snprintf(c, sizeof(c), "%ld", (long)((s_countdownEnd - now + 999) / 1000));
    u8g2.setFont(F_HUGE); uiCenter(48, c);
  } else {
    // camera glyph
    int cx = 64, cy = 33;
    u8g2.drawRFrame(cx - 20, cy - 11, 40, 24, 4);
    u8g2.drawBox(cx - 8, cy - 15, 16, 5);
    u8g2.drawCircle(cx, cy + 1, 8);
    bool flash = now - s_lastShot < 150;
    if (flash) u8g2.drawDisc(cx, cy + 1, 6); else u8g2.drawCircle(cx, cy + 1, 4);
    u8g2.setFont(F_SMALL);
    char n[16]; snprintf(n, sizeof(n), "%d shot%s", s_shots, s_shots == 1 ? "" : "s"); u8g2.drawStr(2, 22, n);
    if (s_wave) uiRight(126, 22, "WAVE");
  }
  u8g2.setFont(F_TINY);
  if (ready) { u8g2.drawStr(0, 63, hidRouteName()); uiRight(127, 63, "SEL shoot  > timer"); }
  else { char p[40]; snprintf(p, sizeof(p), "Pair phone with \"%s\"", S.devName); uiCenter(63, p); }
  npxSetBase(NPX_SOLID, ready ? strip.Color(0, 60, 0) : strip.Color(80, 50, 0), 3);
}
