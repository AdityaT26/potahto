// Bubble level / accelerometer / gyroscope / temperature
#include "Multitool.h"
#include <math.h>

static uint8_t s_page = 0;          // 0 level, 1 accel, 2 gyro, 3 temp
static bool    s_wasCentered = false;

static void drawDots(int n, int sel) {
  int cell = 6, sx = (SCR_W - n * cell) / 2;
  for (int i = 0; i < n; i++) {
    int x = sx + i * cell + 3;
    if (i == sel) u8g2.drawDisc(x, 62, 1); else u8g2.drawPixel(x, 62);
  }
}
static void drawBiBar(int y, const char* label, float val, float scaleMax) {
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(2, y + 7, label);
  const int tx = 14, tw = 86, cx = tx + tw / 2;
  u8g2.drawFrame(tx, y, tw, 9);
  u8g2.drawVLine(cx, y, 9);
  int half = tw / 2 - 2;
  int px = (int)clampv(val * half / scaleMax, (float)-half, (float)half);
  if (px >= 0) u8g2.drawBox(cx, y + 2, px, 5);
  else         u8g2.drawBox(cx + px, y + 2, -px, 5);
  char nb[8]; snprintf(nb, sizeof(nb), "%+d", (int)val);
  u8g2.setFont(F_TINY);
  uiRight(126, y + 7, nb);
}

void motionEnter() { s_page = 0; }

void motionLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_RIGHT)) { s_page = (s_page + 1) % 4; fbNav(); }
  if (btnRepeat(BTN_LEFT))  { s_page = (s_page + 3) % 4; fbNav(); }

  Vec3f a{0,0,0}, g{0,0,0}; float tC = 0;
  bool ok = mpuRead(a, g, tC);

  if (!mpuPresent()) {
    uiHeader("Motion");
    u8g2.setFont(F_UI);
    uiCenter(38, "No MPU6050");
    return;
  }

  static const char* titles[4] = { "Level", "Accel", "Gyro", "Temp" };
  uiHeader(titles[s_page]);

  if (s_page == 0) {                                   // ---- bubble level ----
    float p, r; accelAngles(a, p, r);
    p -= S.accelBias[0]; r -= S.accelBias[1];
    const int cx = 64, cy = 38, R = 20;
    u8g2.drawCircle(cx, cy, R);
    u8g2.drawCircle(cx, cy, 4);
    u8g2.drawHLine(cx - R, cy, 2 * R);
    u8g2.drawVLine(cx, cy - R, 2 * R);
    float sc = (R - 4) / 30.0f;                        // 30 deg -> edge
    int bx = cx + (int)clampv(r * sc, (float)-(R - 4), (float)(R - 4));
    int by = cy - (int)clampv(p * sc, (float)-(R - 4), (float)(R - 4));
    bool centered = fabsf(p) < 3 && fabsf(r) < 3;
    if (centered) u8g2.drawDisc(bx, by, 4); else u8g2.drawCircle(bx, by, 4);

    char t[10];
    u8g2.setFont(F_SMALL);
    snprintf(t, sizeof(t), "P%+d", (int)p); u8g2.drawStr(2, 22, t);
    snprintf(t, sizeof(t), "R%+d", (int)r); uiRight(126, 22, t);

    if (centered && !s_wasCentered) { fbNav(); }
    s_wasCentered = centered;
    npxSetBase(NPX_SOLID, centered ? strip.Color(0,180,40)
              : strip.Color(160, 40, 0), 5);

    if (btnTap(BTN_SELECT)) { mpuLevelHere(); toast("Leveled"); fbConfirm(); }
  }
  else if (s_page == 1) {                              // ---- accelerometer ----
    drawBiBar(22, "X", a.x, 20);
    drawBiBar(34, "Y", a.y, 20);
    drawBiBar(46, "Z", a.z, 20);
    float mag = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
    char m[20]; snprintf(m, sizeof(m), "|a| %.2f m/s2", mag);
    u8g2.setFont(F_SMALL); uiCenter(58, m);
    npxSetBase(NPX_SOLID, strip.Color(0, 40, 120), 4);
  }
  else if (s_page == 2) {                              // ---- gyroscope ----
    drawBiBar(22, "X", g.x, 250);
    drawBiBar(34, "Y", g.y, 250);
    drawBiBar(46, "Z", g.z, 250);
    float mag = sqrtf(g.x*g.x + g.y*g.y + g.z*g.z);
    char m[20]; snprintf(m, sizeof(m), "%.0f dps  SEL=zero", mag);
    u8g2.setFont(F_SMALL); uiCenter(58, m);
    npxSetBase(NPX_SOLID, strip.Color(90, 0, 120), 4);
    if (btnTap(BTN_SELECT)) {
      u8g2.setFont(F_UI); // draw a quick notice this frame
      mpuCalibrateGyro();
      toast("Gyro zeroed"); fbConfirm();
    }
  }
  else {                                               // ---- temperature ----
    char v[10]; snprintf(v, sizeof(v), "%.1f", tC);
    uiBigValue(64, 40, v, "C");
    char f[16]; snprintf(f, sizeof(f), "%.1f F", tC * 9.0f / 5.0f + 32.0f);
    u8g2.setFont(F_SMALL); uiCenter(56, f);
    u8g2.setFont(F_TINY);  uiCenter(20, "MPU die temperature");
    npxSetBase(NPX_SOLID, strip.Color(0, 60, 90), 4);
  }

  drawDots(4, s_page);
  (void)ok;
}
