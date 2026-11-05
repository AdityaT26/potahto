// Heading, rose, cardinal + calibration
#include "Multitool.h"
#include <math.h>

static bool s_cal = false;

static const char* cardinal(float h) {
  static const char* C[8] = { "N","NE","E","SE","S","SW","W","NW" };
  int i = (int)((h + 22.5f) / 45.0f) & 7;
  return C[i];
}

void compassEnter() { s_cal = false; }

void compassLoop() {
  if (wantBack()) {
    if (s_cal) { compassCalFinish(); s_cal = false; }
    fbBack(); goBack(); return;
  }

  if (!compassPresent()) {
    uiHeader("Compass");
    u8g2.setFont(F_UI); uiCenter(38, "No compass");
    return;
  }

  if (btnTap(BTN_SELECT)) {
    if (!s_cal) { compassCalReset(); s_cal = true; toast("Rotate slowly"); fbSelect(); }
    else        { compassCalFinish(); s_cal = false; toast("Calibrated"); fbConfirm(); }
  }
  if (!s_cal && btnTap(BTN_RIGHT)) {   // point at true north, then press RIGHT
    compassSetNorth(); toast("North set"); fbConfirm();
  }

  uiHeader("Compass");

  if (s_cal) {                                   // ---- calibration ----
    compassCalSample();
    npxSetBase(NPX_RAINBOW, 0, 8);
    u8g2.setFont(F_MEDB); uiCenter(32, "CALIBRATING");
    u8g2.setFont(F_SMALL); uiCenter(46, "rotate through all axes");
    // little spinner
    float a = (millis() % 1000) / 1000.0f * 2 * PI;
    int sx = 64 + cosf(a) * 8, sy = 54 + sinf(a) * 6;
    u8g2.drawDisc(sx, sy, 2);
    u8g2.setFont(F_TINY); uiRight(126, 63, "SEL=done");
    return;
  }

  float h = compassHeading();
  if (h < 0) { u8g2.setFont(F_UI); uiCenter(38, "read error"); return; }

  npxSetBase(NPX_SOLID, npxHSV((uint8_t)(h * 255 / 360), 255, 255), 5);

  // rose
  const int cx = 32, cy = 40, R = 20;
  u8g2.drawCircle(cx, cy, R);
  u8g2.drawDisc(cx, cy, 1);
  // fixed top index
  u8g2.drawTriangle(cx - 3, cy - R - 1, cx + 3, cy - R - 1, cx, cy - R + 4);
  // north needle
  float rad = h * PI / 180.0f;
  int nx = cx - (int)(sinf(rad) * (R - 3));
  int ny = cy - (int)(cosf(rad) * (R - 3));
  u8g2.drawLine(cx, cy, nx, ny);
  u8g2.drawDisc(nx, ny, 2);
  u8g2.setFont(F_TINY);
  u8g2.drawStr(nx - 2, ny - 3, "N");

  // heading number + degree ring + cardinal
  char val[6]; snprintf(val, sizeof(val), "%d", (int)(h + 0.5f));
  u8g2.setFont(F_BIG);
  int vw = u8g2.getStrWidth(val);
  int vx = 90 - vw / 2;
  u8g2.drawStr(vx, 44, val);
  u8g2.drawCircle(vx + vw + 4, 26, 2);
  // cardinal centered under the number block
  const char* cd = cardinal(h);
  u8g2.setFont(F_UI);
  u8g2.drawStr(90 - u8g2.getStrWidth(cd) / 2, 58, cd);

  u8g2.setFont(F_TINY);
  u8g2.drawStr(2, 63, S.compassCalibrated ? "SEL=cal  RIGHT=set North"
                                          : "SEL=calibrate first");
}
