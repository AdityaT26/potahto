// "Sonar": fly through the cave with your hand over the ToF sensor as the
// throttle. Closer hand = fly up. Endless.
#include "Multitool.h"

#define CRAFT_X 30

static uint8_t  gapC[128];        // gap center per column (scrolls left)
static float    scrollAcc, spd, craftY, emaMm;
static uint32_t s_dist;
static uint8_t  s_state;          // 0 play, 1 dead
static bool     s_haveMm;

static int gapHalf() { int h = 16 - (int)(s_dist / 220); return h < 8 ? 8 : h; }

static void newColumn() {
  int h = gapHalf();
  int prev = gapC[127];
  int nc = prev + (rand() % 5 - 2);
  nc = clampv(nc, h + 3, 62 - h);
  for (int i = 0; i < 127; i++) gapC[i] = gapC[i + 1];
  gapC[127] = (uint8_t)nc;
}
static void sonarReset() {
  for (int i = 0; i < 128; i++) gapC[i] = 32;
  scrollAcc = 0; spd = 1.4f; craftY = 32; emaMm = 200; s_dist = 0; s_state = 0; s_haveMm = false;
  gBase(NPX_SOLID, strip.Color(0, 80, 120), 5);
}
void sonarEnter() { srand(millis()); sonarReset(); }

void sonarLoop() {
  if (!tofPresent()) {
    u8g2.clearBuffer(); uiHeader("Sonar");
    u8g2.setFont(F_UI); uiCenter(38, "No ToF sensor");
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) gameReturnToHub();
    gfxFlush(); return;
  }

  uint16_t mm;
  if (tofReadMm(mm)) { emaMm += (mm - emaMm) * 0.35f; s_haveMm = true; }

  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(s_dist / 10); gameReturnToHub(); return; }
    // hand distance -> altitude (closer = higher)
    float target = 8 + (clampv(emaMm, 40.0f, 350.0f) - 40.0f) * (48.0f / 310.0f);
    craftY += (target - craftY) * 0.4f;

    spd += 0.0007f; if (spd > 3.0f) spd = 3.0f;
    scrollAcc += spd;
    while (scrollAcc >= 1.0f) { scrollAcc -= 1.0f; newColumn(); s_dist++; }

    int h = gapHalf();
    int top = gapC[CRAFT_X] - h, bot = gapC[CRAFT_X] + h;
    if (craftY < top || craftY > bot) { s_state = 1; gLose(); gameSubmitScore(s_dist / 10); }
    else if ((s_dist % 120) == 0 && s_dist) gSfx(2400, 10);
  } else {
    if (btnTap(BTN_SELECT)) { sonarReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  int h = gapHalf();
  for (int x = 0; x < 128; x++) {
    int top = gapC[x] - h, bot = gapC[x] + h;
    if (top > 0)  u8g2.drawVLine(x, 0, top);
    if (bot < 63) u8g2.drawVLine(x, bot, 64 - bot);
  }
  // craft
  int cyp = (int)craftY;
  u8g2.drawTriangle(CRAFT_X - 3, cyp - 2, CRAFT_X - 3, cyp + 2, CRAFT_X + 4, cyp);
  // hud
  u8g2.setFont(F_TINY);
  char sc[16]; snprintf(sc, sizeof(sc), "%lu", (unsigned long)(s_dist / 10)); u8g2.drawStr(2, 7, sc);
  char hi[12]; snprintf(hi, sizeof(hi), "hi %lu", (unsigned long)hiscoreGet("hsSonar"));
  u8g2.drawStr(126 - u8g2.getStrWidth(hi), 7, hi);

  if (s_state == 1) {
    char l[16]; snprintf(l, sizeof(l), "%lu m", (unsigned long)(s_dist / 10));
    bool best = (s_dist / 10) >= hiscoreGet("hsSonar");
    gfxCenterPanel("CRASH!", l, best ? "NEW BEST!" : "", "SEL=retry  hold=quit");
  }
  gfxFlush();
}
