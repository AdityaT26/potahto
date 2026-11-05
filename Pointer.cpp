// Gyro -> mouse engine + the macro pointer overlay
#include "Multitool.h"

static float    s_accX = 0, s_accY = 0, s_smH = 0, s_smV = 0, s_scAcc = 0;
static float    s_rollRef = 0;
static uint32_t s_lastMs = 0;
static const float SCREEN_GAIN[4] = { 0.7f, 1.0f, 1.35f, 2.0f };

static inline float axisOf(uint8_t i, const Vec3f& v) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
static inline float rollAbs(const Vec3f& a) { return atan2f(axisOf(S.mouseAxisH, a), axisOf(S.mouseAxisV, a)); }

void pointerReset() { s_accX = s_accY = s_smH = s_smV = s_scAcc = 0; s_lastMs = millis(); }
void pointerRecenter() {
  Vec3f a, g; float t;
  if (mpuPresent() && mpuRead(a, g, t)) s_rollRef = rollAbs(a);
  pointerReset();
}
bool pointerClutch() { return S.mouseHoldMoves ? btnDown(BTN_DOWN) : !btnDown(BTN_DOWN); }

bool pointerStep(bool moving, bool scroll) {
  uint32_t now = millis();
  float dt = (now - s_lastMs) / 1000.0f; s_lastMs = now;
  if (dt < 0.005f) dt = 0.005f; else if (dt > 0.1f) dt = 0.1f;
  Vec3f a{0, 0, 0}, g{0, 0, 0}; float tc;
  if (!mpuPresent() || !mpuRead(a, g, tc)) return false;
  if (!moving || !hidReady()) { s_accX = s_accY = s_smH = s_smV = 0; return true; }
  inputMarkActivity();                                 // pointing counts as using it
  if (scroll) {
    float rS = axisOf(S.mouseAxisS, g);
    if (fabsf(rS) < 4) rS = 0;
    if (S.mouseInvertY) rS = -rS;
    s_scAcc += rS * dt * (0.06f + S.scrollSens * 0.10f);
    int w = (int)s_scAcc; s_scAcc -= w;
    if (w) hidMouseMove(0, 0, clampv(w, -20, 20), 0);
    return true;
  }
  float rH = axisOf(S.mouseAxisH, g), rV = axisOf(S.mouseAxisV, g);
  if (fabsf(rH) < 2) rH = 0;
  if (fabsf(rV) < 2) rV = 0;
  if (S.mouseInvertX) rH = -rH;
  if (S.mouseInvertY) rV = -rV;
  if (S.mouseTiltComp) {
    float roll = rollAbs(a) - s_rollRef, c = cosf(roll), s = sinf(roll);
    float h = rH * c - rV * s, v = rH * s + rV * c; rH = h; rV = v;
  }
  float alpha = 1.0f - S.mouseSmooth / 13.0f; if (alpha < 0.15f) alpha = 0.15f;
  s_smH += (rH - s_smH) * alpha; rH = s_smH;
  s_smV += (rV - s_smV) * alpha; rV = s_smV;
  float speed = sqrtf(rH * rH + rV * rV);
  float accel = 1.0f + (S.mouseAccel / 10.0f) * (speed / 130.0f); if (accel > 5.0f) accel = 5.0f;
  float k = S.mouseSens * 0.55f * SCREEN_GAIN[S.mouseScreen & 3] * accel;
  s_accX += rH * dt * k; s_accY += rV * dt * k;
  int dx = (int)s_accX; s_accX -= dx;
  int dy = (int)s_accY; s_accY -= dy;
  dx = clampv(dx, -120, 120); dy = clampv(dy, -120, 120);
  if (dx || dy) hidMouseMove(dx, dy);
  return true;
}

// ---- overlay
static bool     s_on = false, s_down = false, s_dragged = false;
static uint8_t  s_mode = PTR_FREE;
static char     s_title[24];
static uint32_t s_since = 0, s_pressAt = 0;
static int      s_cx = 64, s_cy = 36;                  // decorative on-screen cursor

void pointerStart(uint8_t mode, const char* title) {
  s_on = true; s_mode = mode; s_down = false; s_dragged = false; s_since = millis();
  strlcpy(s_title, title && title[0] ? title : "Pointer", sizeof(s_title));
  inputSetRawSelect(true);
  inputClearFrame();
  pointerReset();
  powerWake();
  fbSelect();
}
bool pointerActive() { return s_on; }
void pointerCancel() {
  if (!s_on) return;
  if (s_down) hidMouseRelease(HB_LEFT);
  s_on = false; inputSetRawSelect(false); inputClearFrame();
  pointerReset();
}

void pointerOverlayLoop() {
  uint32_t now = millis();
  // while the button is held (dragging) the pointer always follows the tilt:
  // a 5-way switch can't hold SELECT and DOWN at the same time
  bool moving = pointerClutch() || s_down;
  if (btnPressed(BTN_SELECT)) { hidMousePress(HB_LEFT); s_down = true; s_pressAt = now; fbNav(); }
  if (btnReleased(BTN_SELECT) && s_down) {
    hidMouseRelease(HB_LEFT); s_down = false;
    if (s_mode == PTR_CLICK || (s_mode == PTR_DRAG && (s_dragged || now - s_pressAt > 250))) { fbConfirm(); pointerCancel(); return; }
  }
  if (btnTap(BTN_RIGHT)) { hidMousePress(HB_RIGHT); hidMouseRelease(HB_RIGHT); fbNav(); }
  if (btnTap(BTN_UP))    { pointerRecenter(); toast("Re-centered"); fbConfirm(); }
  if (btnTap(BTN_LEFT))  { fbBack(); pointerCancel(); return; }
  if (btnLong(BTN_LEFT)) { hidTap(0, HK_ESC); fbCancel(); pointerCancel(); return; }
  if (s_down && now - s_pressAt > 250) s_dragged = true;
  pointerStep(moving, false);
  if (anyButtonDown()) s_since = now;
  if (now - s_since > 60000) { pointerCancel(); return; }

  // ---- draw
  if (!powerScreenOn()) return;
  uiHeader(s_title);
  // a little animated cursor that drifts with the motion state
  int ty = moving ? 30 + (int)(sinf(now / 180.0f) * 3) : 30;
  s_cx += (64 - s_cx) / 4; s_cy += (ty - s_cy) / 3;
  u8g2.drawTriangle(s_cx, s_cy - 8, s_cx, s_cy + 6, s_cx + 9, s_cy + 2);
  u8g2.setDrawColor(0); u8g2.drawTriangle(s_cx + 1, s_cy - 5, s_cx + 1, s_cy + 3, s_cx + 6, s_cy + 1); u8g2.setDrawColor(1);
  u8g2.setFont(F_SMALL);
  const char* st = s_down ? "DRAGGING" : moving ? "moving" : (S.mouseHoldMoves ? "hold DOWN to move" : "moving (DOWN = freeze)");
  uiCenter(48, st);
  u8g2.setFont(F_TINY);
  uiCenter(63, s_mode == PTR_FREE ? "SEL click  > right  < done" : "SEL hold to drag  < cancel");
  if (!hidReady()) { u8g2.drawStr(0, 56, hidRouteName()); }
}
