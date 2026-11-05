// "Tilt Race": tilt to steer, dodge blocks, grab coins.
//
// Uses the accelerometer. Endless, speeds up. (accel axis notes in README)
#include "Multitool.h"

#define NOBS  6
#define NCOIN 3
#define SHIP_W 9
#define SHIP_H 6
#define SHIP_Y 56

static float    shipX;
static float    obX[NOBS], obY[NOBS]; static uint8_t obW[NOBS];
static float    coX[NCOIN], coY[NCOIN]; static bool coOn[NCOIN];
static float    spd;
static uint32_t s_score, s_spawn;
static uint8_t  s_state;   // 0 play, 1 dead

static void spawnObstacle(int i) {
  obW[i] = 8 + rand() % 22;
  obX[i] = rand() % (128 - obW[i]);
  obY[i] = -6 - (rand() % 30);
}
static void dodgeReset() {
  shipX = 60; spd = 1.3f; s_score = 0; s_spawn = 0; s_state = 0;
  for (int i = 0; i < NOBS; i++) { spawnObstacle(i); obY[i] = -40 - i * 22; }
  for (int i = 0; i < NCOIN; i++) coOn[i] = false;
  gBase(NPX_SOLID, strip.Color(0, 60, 90), 5);
}
void dodgeEnter() { srand(millis()); dodgeReset(); }

static bool overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

void dodgeLoop() {
  if (!mpuPresent()) {
    u8g2.clearBuffer(); uiHeader("Tilt Race");
    u8g2.setFont(F_UI); uiCenter(38, "No MPU6050");
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) gameReturnToHub();
    gfxFlush(); return;
  }

  Vec3f a, g; float t; mpuRead(a, g, t);

  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(s_score); gameReturnToHub(); return; }
    shipX += gTiltH(a) * 1.4f;                   // tilt to steer
    shipX = clampv(shipX, 0.0f, (float)(128 - SHIP_W));

    spd += 0.0006f; if (spd > 4.2f) spd = 4.2f;
    s_score++;

    for (int i = 0; i < NOBS; i++) {
      obY[i] += spd;
      if (obY[i] > 64) spawnObstacle(i);
      if (overlap(shipX, SHIP_Y, SHIP_W, SHIP_H, obX[i], obY[i], obW[i], 5)) {
        s_state = 1; gLose(); gameSubmitScore(s_score / 10);
      }
    }
    // coins
    if (millis() - s_spawn > 900) {
      s_spawn = millis();
      for (int i = 0; i < NCOIN; i++) if (!coOn[i]) { coOn[i] = true; coX[i] = 4 + rand() % 120; coY[i] = -4; break; }
    }
    for (int i = 0; i < NCOIN; i++) if (coOn[i]) {
      coY[i] += spd;
      if (coY[i] > 64) coOn[i] = false;
      else if (overlap(shipX, SHIP_Y, SHIP_W, SHIP_H, coX[i] - 2, coY[i] - 2, 4, 4)) {
        coOn[i] = false; s_score += 250; gSfx(2600, 14); gFlash(strip.Color(180, 150, 0), 1, 70);
      }
    }
  } else {
    if (btnTap(BTN_SELECT)) { dodgeReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  u8g2.setFont(F_TINY);
  char sc[16]; snprintf(sc, sizeof(sc), "%lu m", (unsigned long)(s_score / 10));
  u8g2.drawStr(2, 7, sc);
  char h[12]; snprintf(h, sizeof(h), "hi %lu", (unsigned long)hiscoreGet("hsDodge"));
  u8g2.drawStr(126 - u8g2.getStrWidth(h), 7, h);
  u8g2.drawHLine(0, 9, 128);
  for (int i = 0; i < NOBS; i++) u8g2.drawBox((int)obX[i], (int)obY[i], obW[i], 5);
  for (int i = 0; i < NCOIN; i++) if (coOn[i]) u8g2.drawDisc((int)coX[i], (int)coY[i], 2);
  // ship: little triangle
  int sx = (int)shipX;
  u8g2.drawTriangle(sx, SHIP_Y + SHIP_H, sx + SHIP_W, SHIP_Y + SHIP_H, sx + SHIP_W / 2, SHIP_Y);

  if (s_state == 1) {
    char l[16]; snprintf(l, sizeof(l), "%lu m", (unsigned long)(s_score / 10));
    bool best = (s_score / 10) >= hiscoreGet("hsDodge");
    gfxCenterPanel("CRASH!", l, best ? "NEW BEST!" : "", "SEL=retry  hold=quit");
  }
  gfxFlush();
}
