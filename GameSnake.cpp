// Snake
#include "Multitool.h"

#define SN_CELL 4
#define SN_GW   32
#define SN_GH   13
#define SN_OX   0
#define SN_OY   12
#define SN_MAX  (SN_GW * SN_GH)

static uint8_t  sx[SN_MAX], sy[SN_MAX];
static int      s_len;
static int      s_dx, s_dy, s_ndx, s_ndy;
static uint8_t  fx, fy;
static uint32_t s_step, s_last, s_score;
static uint8_t  s_state;   // 0 play, 1 dead

static void placeFood() {
  while (true) {
    uint8_t rx = rand() % SN_GW, ry = rand() % SN_GH;
    bool onBody = false;
    for (int i = 0; i < s_len; i++) if (sx[i] == rx && sy[i] == ry) { onBody = true; break; }
    if (!onBody) { fx = rx; fy = ry; return; }
  }
}
static void snakeReset() {
  s_len = 4;
  for (int i = 0; i < s_len; i++) { sx[i] = 6 + i; sy[i] = SN_GH / 2; }
  s_dx = 1; s_dy = 0; s_ndx = 1; s_ndy = 0;
  s_score = 0; s_step = 180; s_last = millis(); s_state = 0;
  placeFood();
  gBase(NPX_SOLID, strip.Color(0, 120, 0), 5);
}
void snakeEnter() { srand(millis()); snakeReset(); }

static void snakeStep() {
  s_dx = s_ndx; s_dy = s_ndy;
  int hx = sx[s_len - 1] + s_dx, hy = sy[s_len - 1] + s_dy;
  if (hx < 0 || hx >= SN_GW || hy < 0 || hy >= SN_GH) { s_state = 1; gLose(); return; }
  bool grow = (hx == fx && hy == fy);
  for (int i = (grow ? 0 : 1); i < s_len; i++)
    if (sx[i] == hx && sy[i] == hy) { s_state = 1; gLose(); return; }
  if (!grow) {
    for (int i = 0; i < s_len - 1; i++) { sx[i] = sx[i + 1]; sy[i] = sy[i + 1]; }
    s_len--;
  }
  sx[s_len] = hx; sy[s_len] = hy; s_len++;
  if (grow) {
    s_score++;
    if (s_step > 75) s_step -= 4;
    gSfx(1900, 18);
    gFlash(strip.Color(0, 200, 0), 1, 90);
    if (s_len < SN_MAX) placeFood();
  }
}

void snakeLoop() {
  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameReturnToHub(); return; }
    if (btnPressed(BTN_UP)    && s_dy == 0) { s_ndx = 0; s_ndy = -1; }
    if (btnPressed(BTN_DOWN)  && s_dy == 0) { s_ndx = 0; s_ndy = 1; }
    if (btnPressed(BTN_LEFT)  && s_dx == 0) { s_ndx = -1; s_ndy = 0; }
    if (btnPressed(BTN_RIGHT) && s_dx == 0) { s_ndx = 1; s_ndy = 0; }
    if (millis() - s_last >= s_step) { s_last = millis(); snakeStep(); }
  } else {
    if (btnTap(BTN_SELECT)) { snakeReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameSubmitScore(s_score); gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  u8g2.setFont(F_TINY);
  char t[20]; snprintf(t, sizeof(t), "SNAKE  %lu", (unsigned long)s_score);
  u8g2.drawStr(2, 8, t);
  char h[12]; snprintf(h, sizeof(h), "hi %lu", (unsigned long)hiscoreGet("hsSnake"));
  u8g2.drawStr(126 - u8g2.getStrWidth(h), 8, h);
  u8g2.drawHLine(0, 10, 128);
  u8g2.drawFrame(SN_OX + fx * SN_CELL, SN_OY + fy * SN_CELL, SN_CELL, SN_CELL);
  for (int i = 0; i < s_len; i++)
    u8g2.drawBox(SN_OX + sx[i] * SN_CELL, SN_OY + sy[i] * SN_CELL, SN_CELL - 1, SN_CELL - 1);

  if (s_state == 1) {
    char sc[16]; snprintf(sc, sizeof(sc), "Score %lu", (unsigned long)s_score);
    bool best = s_score >= hiscoreGet("hsSnake");
    gfxCenterPanel("GAME OVER", sc, best ? "NEW BEST!" : "", "SEL=retry  hold=quit");
  }
  gfxFlush();
}
