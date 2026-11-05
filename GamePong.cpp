// You (left) vs AI (right), first to 7
#include "Multitool.h"

#define PAD_H 14
#define PAD_W 3
#define P_X   2
#define A_X   (128 - 2 - PAD_W)
#define WIN_SC 7

static float    bx, by, bvx, bvy;
static float    py, ay;
static int      ps, as, rally, bestRally;
static uint8_t  state;          // 0 play, 1 serve-wait, 2 over
static uint32_t serveAt;
static bool     playerWon;

static void serve(int dir) {
  bx = 64; by = 32;
  bvx = dir * 1.7f;
  bvy = ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
  rally = 0;
  state = 1; serveAt = millis() + 600;
}
static void pongReset() {
  ps = as = 0; bestRally = 0;
  py = ay = 25;
  serve((rand() & 1) ? 1 : -1);
  gBase(NPX_SOLID, strip.Color(0, 40, 120), 5);
}
void pongEnter() { srand(millis()); pongReset(); }

static void point(bool playerScored) {
  if (playerScored) ps++; else as++;
  if (rally > bestRally) bestRally = rally;
  gSfx(playerScored ? 1600 : 300, 60);
  gHap(30, 5);
  if (ps >= WIN_SC || as >= WIN_SC) {
    state = 2; playerWon = (ps >= WIN_SC);
    if (playerWon) { gWin(); } else { gLose(); }
    gameSubmitScore(bestRally);
  } else {
    serve(playerScored ? 1 : -1);
  }
}

void pongLoop() {
  if (state == 2) {
    if (btnTap(BTN_SELECT)) { pongReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  } else {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(bestRally); gameReturnToHub(); return; }
    if (btnDown(BTN_UP))   py -= 3.2f;
    if (btnDown(BTN_DOWN)) py += 3.2f;
    py = clampv(py, 0.0f, (float)(64 - PAD_H));

    if (state == 1) {                      // serve pause
      if (millis() >= serveAt) state = 0;
    } else {                               // active rally
      // AI: chase ball when it approaches, else drift to center
      float aiSpeed = 2.0f + (ps + as) * 0.12f; if (aiSpeed > 4.0f) aiSpeed = 4.0f;
      float target = (bvx > 0) ? (by - PAD_H / 2) : (25.0f);
      float d = target - ay;
      ay += clampv(d, -aiSpeed, aiSpeed);
      ay = clampv(ay, 0.0f, (float)(64 - PAD_H));

      bx += bvx; by += bvy;
      if (by < 1) { by = 1; bvy = -bvy; }
      if (by > 62) { by = 62; bvy = -bvy; }

      if (bvx < 0 && bx <= P_X + PAD_W && bx > P_X - 2 && by >= py && by <= py + PAD_H) {
        bx = P_X + PAD_W; bvx = -bvx * 1.06f;
        bvy += ((by - (py + PAD_H / 2)) / (PAD_H / 2)) * 1.6f;
        rally++; gSfx(2200, 12);
      }
      if (bvx > 0 && bx >= A_X - 1 && bx <= A_X + PAD_W && by >= ay && by <= ay + PAD_H) {
        bx = A_X - 1; bvx = -bvx * 1.06f;
        bvy += ((by - (ay + PAD_H / 2)) / (PAD_H / 2)) * 1.6f;
        rally++; gSfx(2000, 12);
      }
      bvx = clampv(bvx, -4.5f, 4.5f); bvy = clampv(bvy, -3.5f, 3.5f);

      if (bx < -2) point(false);
      else if (bx > 130) point(true);
    }
  }

  // ---- draw
  u8g2.clearBuffer();
  // center net
  for (int y = 2; y < 64; y += 6) u8g2.drawVLine(64, y, 3);
  // score
  u8g2.setFont(F_MEDB);
  char ls[4], rs[4]; snprintf(ls, sizeof(ls), "%d", ps); snprintf(rs, sizeof(rs), "%d", as);
  u8g2.drawStr(52 - u8g2.getStrWidth(ls), 12, ls);
  u8g2.drawStr(76, 12, rs);
  // paddles + ball
  u8g2.drawBox(P_X, (int)py, PAD_W, PAD_H);
  u8g2.drawBox(A_X, (int)ay, PAD_W, PAD_H);
  u8g2.drawBox((int)bx - 1, (int)by - 1, 2, 2);

  if (state == 2) {
    char sc[16]; snprintf(sc, sizeof(sc), "%d - %d", ps, as);
    gfxCenterPanel(playerWon ? "YOU WIN!" : "YOU LOSE", sc, "SEL=rematch", "hold=quit");
  }
  gfxFlush();
}
