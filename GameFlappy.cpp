// One-button infinite flyer
#include "Multitool.h"

#define FB_X    30
#define FB_W    6
#define FB_H    5
#define PIPE_W  10
#define NPIPE   3
#define SPACING 48
#define GND_Y   62

static float    by, vy;
static float    px[NPIPE];
static int      gapY[NPIPE];
static bool     passed[NPIPE];
static float    spd;
static uint32_t s_score;
static uint8_t  state;      // 0 ready, 1 play, 2 dead

static int curGap() { int g = 26 - (int)(s_score / 3); return g < 18 ? 18 : g; }

static void newPipe(int i, float x) {
  int g = curGap();
  px[i] = x;
  gapY[i] = 6 + rand() % (64 - g - 12);
  passed[i] = false;
}
static void flappyReset() {
  by = 30; vy = 0; spd = 1.6f; s_score = 0; state = 0;
  for (int i = 0; i < NPIPE; i++) newPipe(i, 128 + i * SPACING);
  gBase(NPX_SOLID, strip.Color(0, 90, 120), 5);
}
void flappyEnter() { srand(millis()); flappyReset(); }

static void flap() { vy = -3.1f; gSfx(2000, 14); gHap(8, 3); }

static bool hitsPipe() {
  int g = curGap();
  for (int i = 0; i < NPIPE; i++) {
    int x = (int)px[i];
    if (FB_X + FB_W > x && FB_X < x + PIPE_W) {
      if (by < gapY[i] || by + FB_H > gapY[i] + g) return true;
    }
  }
  return false;
}

void flappyLoop() {
  bool tapFlap = btnPressed(BTN_UP) || btnTap(BTN_SELECT);

  if (state == 0) {
    if (btnLong(BTN_SELECT)) { gameReturnToHub(); return; }
    by = 30 + sinf(millis() / 200.0f) * 3;
    if (tapFlap) { state = 1; flap(); }
  } else if (state == 1) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(s_score); gameReturnToHub(); return; }
    if (tapFlap) flap();
    vy += 0.45f; by += vy;
    for (int i = 0; i < NPIPE; i++) {
      px[i] -= spd;
      if (!passed[i] && px[i] + PIPE_W < FB_X) {
        passed[i] = true; s_score++; gSfx(2600, 16);
        gFlash(strip.Color(0, 180, 60), 1, 70);
        spd = 1.6f + s_score * 0.03f; if (spd > 3.2f) spd = 3.2f;
      }
      if (px[i] < -PIPE_W) {
        float mx = px[0]; for (int k = 1; k < NPIPE; k++) if (px[k] > mx) mx = px[k];
        newPipe(i, mx + SPACING);
      }
    }
    if (by + FB_H >= GND_Y || by <= 0 || hitsPipe()) {
      state = 2; gLose(); gameSubmitScore(s_score);
    }
  } else {
    if (btnTap(BTN_SELECT)) { flappyReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  int g = curGap();
  for (int i = 0; i < NPIPE; i++) {
    int x = (int)px[i];
    if (x > 128 || x + PIPE_W < 0) continue;
    u8g2.drawBox(x, 0, PIPE_W, gapY[i]);
    u8g2.drawBox(x, gapY[i] + g, PIPE_W, GND_Y - (gapY[i] + g));
  }
  u8g2.drawHLine(0, GND_Y, 128);
  // bird
  u8g2.drawRBox(FB_X, (int)by, FB_W, FB_H, 1);
  u8g2.setDrawColor(0); u8g2.drawPixel(FB_X + FB_W - 2, (int)by + 1); u8g2.setDrawColor(1);
  // score
  u8g2.setFont(F_MEDB);
  char sc[6]; snprintf(sc, sizeof(sc), "%lu", (unsigned long)s_score);
  u8g2.drawStr(64 - u8g2.getStrWidth(sc) / 2, 12, sc);

  if (state == 0) { u8g2.setFont(F_TINY); uiCenter(56, "UP / SELECT to flap"); }
  else if (state == 2) {
    char l[16]; snprintf(l, sizeof(l), "Score %lu", (unsigned long)s_score);
    bool best = s_score >= hiscoreGet("hsFlappy");
    gfxCenterPanel("SPLAT!", l, best ? "NEW BEST!" : "", "SEL=retry  hold=quit");
  }
  gfxFlush();
}
