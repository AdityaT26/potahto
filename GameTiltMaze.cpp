// "Tilt Maze": roll the ball to the exit by tilting.
//
// Uses the accelerometer. Generated maze each level, timed, endless.
// (Tilt axis + inversion are configurable in Games > Settings > Controls.)
#include "Multitool.h"

#define CW  15
#define CH  6
#define TW  (2 * CW + 1)      // 31
#define TH  (2 * CH + 1)      // 13
#define TS  4
#define OX  2
#define OY  10

static uint8_t wall[TH][TW];
static float   bx, by, bvx, bvy;
static int     s_level;
static uint32_t s_deadline;
static uint8_t s_state;       // 0 play, 1 dead, 2 cleared(flash)
static uint32_t s_clrAt;

static void genMaze() {
  for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) wall[y][x] = 1;
  static bool vis[CH][CW];
  memset(vis, 0, sizeof(vis));
  static uint8_t stk[CW * CH][2];
  int sp = 0;
  vis[0][0] = true; wall[1][1] = 0; stk[sp][0] = 0; stk[sp][1] = 0; sp++;
  const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
  while (sp > 0) {
    int cx = stk[sp - 1][0], cy = stk[sp - 1][1];
    int order[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) { int j = rand() % (i + 1); int t = order[i]; order[i] = order[j]; order[j] = t; }
    bool moved = false;
    for (int k = 0; k < 4; k++) {
      int d = order[k], nx = cx + dx[d], ny = cy + dy[d];
      if (nx < 0 || nx >= CW || ny < 0 || ny >= CH || vis[ny][nx]) continue;
      vis[ny][nx] = true;
      wall[2 * cy + 1 + dy[d]][2 * cx + 1 + dx[d]] = 0;   // wall between
      wall[2 * ny + 1][2 * nx + 1] = 0;                   // neighbour cell
      stk[sp][0] = nx; stk[sp][1] = ny; sp++;
      moved = true; break;
    }
    if (!moved) sp--;
  }
}
static void newLevel(bool first) {
  genMaze();
  bx = OX + 1 * TS + TS / 2; by = OY + 1 * TS + TS / 2;
  bvx = bvy = 0;
  s_deadline = millis() + 30000;
  s_state = 0;
  if (first) gBase(NPX_SOLID, strip.Color(0, 90, 70), 5);
}
void tmazeEnter() { srand(millis()); s_level = 1; newLevel(true); }

static bool wallPix(float px, float py) {
  int tx = (int)((px - OX) / TS), ty = (int)((py - OY) / TS);
  if (tx < 0 || tx >= TW || ty < 0 || ty >= TH) return true;
  return wall[ty][tx];
}
static bool blocked(float px, float py) {
  const float r = 1.4f;
  return wallPix(px - r, py - r) || wallPix(px + r, py - r) ||
         wallPix(px - r, py + r) || wallPix(px + r, py + r);
}

void tmazeLoop() {
  if (!mpuPresent()) {
    u8g2.clearBuffer(); uiHeader("Tilt Maze");
    u8g2.setFont(F_UI); uiCenter(38, "No MPU6050");
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) gameReturnToHub();
    gfxFlush(); return;
  }

  Vec3f a, g; float t; mpuRead(a, g, t);
  uint32_t now = millis();

  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(s_level - 1); gameReturnToHub(); return; }
    bvx += gTiltH(a) * 0.05f;
    bvy += gTiltV(a) * 0.05f;
    bvx *= 0.82f; bvy *= 0.82f;
    bvx = clampv(bvx, -2.2f, 2.2f); bvy = clampv(bvy, -2.2f, 2.2f);
    float nx = bx + bvx; if (!blocked(nx, by)) bx = nx; else bvx = 0;
    float ny = by + bvy; if (!blocked(bx, ny)) by = ny; else bvy = 0;

    float ex = OX + (TW - 2) * TS + TS / 2, ey = OY + (TH - 2) * TS + TS / 2;
    if (fabsf(bx - ex) < 3 && fabsf(by - ey) < 3) {
      s_state = 2; s_clrAt = now; s_level++; gWin();
    } else if ((int32_t)(s_deadline - now) <= 0) {
      s_state = 1; gLose(); gameSubmitScore(s_level - 1);
    }
  } else if (s_state == 2) {
    if (now - s_clrAt > 500) newLevel(false);
  } else {
    if (btnTap(BTN_SELECT)) { s_level = 1; newLevel(true); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  u8g2.setFont(F_TINY);
  char l[16]; snprintf(l, sizeof(l), "Lvl %d", s_level); u8g2.drawStr(2, 7, l);
  int tleft = (s_state == 0) ? (int)((s_deadline - now) / 1000) : 0;
  char tm[8]; snprintf(tm, sizeof(tm), "%ds", tleft); u8g2.drawStr(126 - u8g2.getStrWidth(tm), 7, tm);
  for (int y = 0; y < TH; y++)
    for (int x = 0; x < TW; x++)
      if (wall[y][x]) u8g2.drawBox(OX + x * TS, OY + y * TS, TS, TS);
  // exit marker
  int exI = OX + (TW - 2) * TS, eyI = OY + (TH - 2) * TS;
  u8g2.drawFrame(exI, eyI, TS, TS);
  // ball
  u8g2.drawDisc((int)bx, (int)by, 1);

  if (s_state == 1) {
    char sc[16]; snprintf(sc, sizeof(sc), "Reached lvl %d", s_level);
    gfxCenterPanel("TIME UP", sc, (uint32_t)(s_level - 1) >= hiscoreGet("hsMaze") ? "BEST!" : "", "SEL=retry  hold=quit");
  }
  gfxFlush();
}
