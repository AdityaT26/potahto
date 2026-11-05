// Neopixel torch: hue / brightness / mode (+ gyro hue)
#include "Multitool.h"

static const char* MODE_NAME[LM_COUNT] = { "Solid", "Breathe", "Rainbow", "Gyro" };
// Torch: its own brightness (Light > Bri, not the status-LED brightness), stays
// lit with the screen off (hold SELECT) and turns itself off after
// Settings > Light > "Torch off after" minutes without a button press.
static bool s_off = false;

void lightEnter() { s_off = false; }
void lightExit()  { settingsSaveSoon(); }   // menu re-applies its own idle base

void lightLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }

  // mode cycle
  if (btnTap(BTN_SELECT)) {
    S.lightMode = (S.lightMode + 1) % LM_COUNT;
    fbSelect(); settingsSaveSoon();
  }
  // brightness (master neopixel brightness)
  if (btnRepeat(BTN_UP))   { S.lightBright = clampv<int>(S.lightBright + 12, 4, 255); fbNav(); settingsSaveSoon(); }
  if (btnRepeat(BTN_DOWN)) { S.lightBright = clampv<int>(S.lightBright - 12, 4, 255); fbNav(); settingsSaveSoon(); }
  // hue via buttons (solid / breathe only)
  bool hueByBtn = (S.lightMode == LM_SOLID || S.lightMode == LM_BREATHE);
  if (hueByBtn) {
    if (btnRepeat(BTN_RIGHT)) { S.lightHue += 6; fbNav(); settingsSaveSoon(); }
    if (btnRepeat(BTN_LEFT))  { S.lightHue -= 6; fbNav(); settingsSaveSoon(); }
  }
  // hue via gyro (tilt) in Gyro mode
  if (S.lightMode == LM_GYRO && mpuPresent()) {
    Vec3f a, g; float t;
    if (mpuRead(a, g, t)) {
      float p, r; accelAngles(a, p, r);
      int h = (int)((r + 90) * 255.0f / 180.0f);
      S.lightHue = (uint8_t)clampv(h, 0, 255);
    }
  }

  // auto-off timer; any button relights it
  bool any = false; for (uint8_t b = 0; b < BTN_COUNT; b++) if (btnPressed(b)) any = true;
  if (any) s_off = false;
  if (S.torchOffM && millis() - lastActivityMs() > (uint32_t)S.torchOffM * 60000UL) s_off = true;
  if (s_off) { npxOff(); }
  else npxKeepOnBright(S.lightBright);
  // apply to the pixel
  if (!s_off) switch (S.lightMode) {
    case LM_SOLID:   npxSetBase(NPX_SOLID,   npxHSV(S.lightHue,255,255), 5); break;
    case LM_BREATHE: npxSetBase(NPX_BREATHE, npxHSV(S.lightHue,255,255), 4); break;
    case LM_RAINBOW: npxSetBase(NPX_RAINBOW, 0, 6); break;
    case LM_GYRO:    npxSetBase(NPX_SOLID,   npxHSV(S.lightHue,255,255), 5); break;
  }

  // ---- UI
  uiHeader("Light");
  u8g2.setFont(F_UI);
  char mline[20]; snprintf(mline, sizeof(mline), s_off ? "Off (timer)" : "Mode: %s", MODE_NAME[S.lightMode]);
  u8g2.drawStr(4, 24, mline);

  // hue row
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(4, 40, "Hue");
  u8g2.drawFrame(30, 33, 80, 8);
  int hx = 30 + (S.lightHue * 79) / 255;
  u8g2.drawBox(hx - 1, 31, 3, 12);                    // hue marker
  bool hueActive = hueByBtn || S.lightMode == LM_GYRO;
  if (!hueActive) { u8g2.setFont(F_TINY); uiRight(126, 39, "auto"); }

  // brightness row
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(4, 54, "Bri");
  uiProgressBar(30, 47, 80, 8, S.lightBright / 255.0f);
  char pb[8]; snprintf(pb, sizeof(pb), "%d%%", (S.lightBright * 100) / 255);
  u8g2.setFont(F_TINY); uiRight(126, 53, pb);

  u8g2.setFont(F_TINY);
  uiCenter(63, S.lightMode == LM_GYRO ? "tilt=hue  UD=bri  SEL=mode"
                                      : "LR=hue  UD=bri  SEL=mode");
}
