// "Blaster": rotate the device to aim, shoot the targets.
//
// Uses the gyroscope. 45-second score attack with combos.
#include "Multitool.h"

#define NT 4
#define ROUND_MS 45000

static float    cx, cy;                 // crosshair
static float    tx[NT], ty[NT]; static uint32_t tBorn[NT]; static bool tOn[NT];
static uint32_t s_score, s_endAt, s_lastSpawn, s_lastMs;
static int      s_combo;
static uint8_t  s_state;   // 0 play, 1 over

static void spawnTarget() {
  for (int i = 0; i < NT; i++) if (!tOn[i]) {
    tOn[i] = true; tx[i] = 10 + rand() % 108; ty[i] = 14 + rand() % 44; tBorn[i] = millis();
    return;
  }
}
static void blasterReset() {
  cx = 64; cy = 36; s_score = 0; s_combo = 0; s_state = 0;
  for (int i = 0; i < NT; i++) tOn[i] = false;
  s_endAt = millis() + ROUND_MS; s_lastSpawn = 0; s_lastMs = millis();
  spawnTarget(); spawnTarget();
  gBase(NPX_SOLID, strip.Color(90, 0, 60), 5);
}
void blasterEnter() { srand(millis()); blasterReset(); }

static void shoot() {
  int best = -1; float bd = 1e9f;
  for (int i = 0; i < NT; i++) if (tOn[i]) {
    float dx = cx - tx[i], dy = cy - ty[i], d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  if (best >= 0 && bd <= 64) {            // within ~8px
    tOn[best] = false;
    s_combo++;
    s_score += 100 + (s_combo - 1) * 25;
    gSfx(2400, 18); gFlash(strip.Color(0, 200, 80), 1, 70); gHap(12, 4);
    spawnTarget();
  } else {
    s_combo = 0; gSfx(220, 40);
  }
}

void blasterLoop() {
  if (!mpuPresent()) {
    u8g2.clearBuffer(); uiHeader("Blaster");
    u8g2.setFont(F_UI); uiCenter(38, "No MPU6050");
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) gameReturnToHub();
    gfxFlush(); return;
  }

  uint32_t now = millis();
  float dt = (now - s_lastMs) / 1000.0f; s_lastMs = now;
  if (dt > 0.1f) dt = 0.1f;
  Vec3f a, g; float t; mpuRead(a, g, t);

  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(s_score); gameReturnToHub(); return; }
    if (btnTap(BTN_UP)) { cx = 64; cy = 36; gClick(); }        // recenter aim
    cx += gAimH(g) * dt * 3.0f;
    cy += gAimV(g) * dt * 3.0f;
    cx = clampv(cx, 2.0f, 125.0f);
    cy = clampv(cy, 12.0f, 61.0f);
    if (btnTap(BTN_SELECT) || btnPressed(BTN_RIGHT) || btnPressed(BTN_LEFT)) shoot();

    for (int i = 0; i < NT; i++) if (tOn[i] && now - tBorn[i] > 2600) { tOn[i] = false; s_combo = 0; }
    if (now - s_lastSpawn > 700) { s_lastSpawn = now; spawnTarget(); }
    if ((int32_t)(s_endAt - now) <= 0) { s_state = 1; gWin(); gameSubmitScore(s_score); }
  } else {
    if (btnTap(BTN_SELECT)) { blasterReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  u8g2.setFont(F_TINY);
  char sc[16]; snprintf(sc, sizeof(sc), "%lu", (unsigned long)s_score); u8g2.drawStr(2, 7, sc);
  if (s_combo > 1) { char cb[8]; snprintf(cb, sizeof(cb), "x%d", s_combo); u8g2.drawStr(40, 7, cb); }
  int tleft = s_state ? 0 : (int)((s_endAt - now) / 1000);
  char tm[8]; snprintf(tm, sizeof(tm), "%ds", tleft); u8g2.drawStr(126 - u8g2.getStrWidth(tm), 7, tm);
  u8g2.drawHLine(0, 9, 128);
  // time bar
  int bw = s_state ? 0 : (int)(124L * (s_endAt - now) / ROUND_MS);
  u8g2.drawBox(2, 10, clampv(bw, 0, 124), 1);
  for (int i = 0; i < NT; i++) if (tOn[i]) {
    int r = 5 - (int)((now - tBorn[i]) / 900); if (r < 2) r = 2;
    u8g2.drawCircle((int)tx[i], (int)ty[i], 5);
    u8g2.drawDisc((int)tx[i], (int)ty[i], r > 3 ? 2 : 1);
  }
  // crosshair
  u8g2.drawCircle((int)cx, (int)cy, 3);
  u8g2.drawHLine((int)cx - 6, (int)cy, 13);
  u8g2.drawVLine((int)cx, (int)cy - 6, 13);

  if (s_state == 1) {
    char l[16]; snprintf(l, sizeof(l), "Score %lu", (unsigned long)s_score);
    gfxCenterPanel("TIME!", l, s_score >= hiscoreGet("hsBlast") ? "NEW BEST!" : "", "SEL=retry  hold=quit");
  }
  gfxFlush();
}
