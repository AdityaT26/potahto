// Portrait (64x128) Tetris. Rotate the device 90 deg.
//
// If the controls feel rotated the wrong way, flip TET_ROT (U8G2_R1<->U8G2_R3)
// and the device the other way; the button map below is for R1.
#include "Multitool.h"

#define TET_ROT     U8G2_R1
#define FW          10
#define FH          20
#define CELL        6
#define FX          2       // field origin x (portrait coords)
#define FY          8

// D-pad map for the rotated view
#define K_LEFT   BTN_UP
#define K_RIGHT  BTN_DOWN
#define K_ROT    BTN_RIGHT
#define K_SOFT   BTN_LEFT

// 7 tetrominoes x 4 rotations, 4x4 bit masks (bit = row*4 + col)
static const uint16_t PIECE[7][4] = {
  { 0x00F0, 0x4444, 0x0F00, 0x2222 }, // I
  { 0x0066, 0x0066, 0x0066, 0x0066 }, // O
  { 0x0072, 0x0262, 0x0270, 0x0232 }, // T
  { 0x0036, 0x0462, 0x0360, 0x0231 }, // S
  { 0x0063, 0x0264, 0x0630, 0x0132 }, // Z
  { 0x0071, 0x0226, 0x0470, 0x0322 }, // J
  { 0x0074, 0x0622, 0x0170, 0x0223 }, // L
};

static uint8_t  grid[FH][FW];
static int      s_pc, s_rot, s_px, s_py, s_next;
static uint32_t s_score; static int s_lines, s_level;
static uint32_t s_grav, s_lastDrop;
static uint8_t  s_state;   // 0 play, 1 over

static bool collide(int pc, int rot, int px, int py) {
  uint16_t m = PIECE[pc][rot];
  for (int b = 0; b < 16; b++) {
    if (!(m & (1 << b))) continue;
    int c = px + (b & 3), r = py + (b >> 2);
    if (c < 0 || c >= FW || r >= FH) return true;
    if (r >= 0 && grid[r][c]) return true;
  }
  return false;
}
static void spawn() {
  s_pc = s_next; s_next = rand() % 7;
  s_rot = 0; s_px = 3; s_py = 0;
  if (collide(s_pc, s_rot, s_px, s_py)) { s_state = 1; gLose(); gameSubmitScore(s_score); }
}
static void tetrisReset() {
  memset(grid, 0, sizeof(grid));
  s_score = 0; s_lines = 0; s_level = 1; s_grav = 700; s_state = 0;
  s_next = rand() % 7; spawn();
  s_lastDrop = millis();
  gBase(NPX_SOLID, strip.Color(60, 0, 90), 5);
}
void tetrisEnter() {
  srand(millis());
  u8g2.setDisplayRotation(TET_ROT);
  inputSetFlip(false);
  gfxResetShadow();
  tetrisReset();
}

static void lockAndClear() {
  uint16_t m = PIECE[s_pc][s_rot];
  for (int b = 0; b < 16; b++) {
    if (!(m & (1 << b))) continue;
    int c = s_px + (b & 3), r = s_py + (b >> 2);
    if (r >= 0 && r < FH && c >= 0 && c < FW) grid[r][c] = 1;
  }
  int cleared = 0;
  for (int r = FH - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < FW; c++) if (!grid[r][c]) { full = false; break; }
    if (full) {
      cleared++;
      for (int rr = r; rr > 0; rr--) for (int c = 0; c < FW; c++) grid[rr][c] = grid[rr - 1][c];
      for (int c = 0; c < FW; c++) grid[0][c] = 0;
      r++;
    }
  }
  if (cleared) {
    static const int pts[5] = { 0, 100, 300, 500, 800 };
    s_score += pts[cleared] * s_level;
    s_lines += cleared;
    s_level = 1 + s_lines / 10;
    s_grav = 700 - (s_level - 1) * 55; if (s_grav < 90) s_grav = 90;
    gSfx(cleared >= 4 ? 2600 : 1800, cleared >= 4 ? 120 : 45);
    gHap(cleared >= 4 ? 60 : 25, cleared >= 4 ? 8 : 5);
    gFlash(cleared >= 4 ? strip.Color(0, 200, 200) : strip.Color(0, 150, 40), cleared, 90);
  }
  spawn();
}
static void tryRotate() {
  int nr = (s_rot + 1) & 3;
  static const int kicks[5] = { 0, -1, 1, -2, 2 };
  for (int i = 0; i < 5; i++) {
    if (!collide(s_pc, nr, s_px + kicks[i], 0 + s_py)) { s_rot = nr; s_px += kicks[i]; gSfx(2400, 8); return; }
  }
}

void tetrisLoop() {
  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(s_score); gameReturnToHub(); return; }
    if (btnRepeat(K_LEFT)  && !collide(s_pc, s_rot, s_px - 1, s_py)) { s_px--; gClick(); }
    if (btnRepeat(K_RIGHT) && !collide(s_pc, s_rot, s_px + 1, s_py)) { s_px++; gClick(); }
    if (btnPressed(K_ROT)) tryRotate();
    if (btnTap(BTN_SELECT)) {                       // hard drop
      while (!collide(s_pc, s_rot, s_px, s_py + 1)) s_py++;
      gSfx(1400, 20); gHap(15, 5);
      lockAndClear(); s_lastDrop = millis();
    }
    uint32_t interval = btnDown(K_SOFT) ? 45 : s_grav;
    if (millis() - s_lastDrop >= interval) {
      s_lastDrop = millis();
      if (!collide(s_pc, s_rot, s_px, s_py + 1)) s_py++;
      else lockAndClear();
    }
  } else {
    if (btnTap(BTN_SELECT)) { tetrisReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(K_SOFT)) { gameReturnToHub(); return; }
  }

  // ---- draw (portrait 64x128)
  u8g2.clearBuffer();
  u8g2.setFont(F_TINY);
  char t[16]; snprintf(t, sizeof(t), "L%d %lu", s_level, (unsigned long)s_score);
  u8g2.drawStr(1, 6, t);
  u8g2.drawFrame(FX - 1, FY - 1, FW * CELL + 2, FH * CELL + 2);
  for (int r = 0; r < FH; r++)
    for (int c = 0; c < FW; c++)
      if (grid[r][c]) u8g2.drawBox(FX + c * CELL, FY + r * CELL, CELL - 1, CELL - 1);
  if (s_state == 0) {
    uint16_t m = PIECE[s_pc][s_rot];
    for (int b = 0; b < 16; b++) {
      if (!(m & (1 << b))) continue;
      int c = s_px + (b & 3), r = s_py + (b >> 2);
      if (r >= 0) u8g2.drawBox(FX + c * CELL, FY + r * CELL, CELL - 1, CELL - 1);
    }
  } else {
    char sc[14]; snprintf(sc, sizeof(sc), "%lu", (unsigned long)s_score);
    gfxCenterPanel("GAME OVER", sc, s_score >= hiscoreGet("hsTetris") ? "BEST!" : "", "SEL=retry");
  }
  gfxFlush();
}
