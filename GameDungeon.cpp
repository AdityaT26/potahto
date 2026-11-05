// Infinite roguelike: fight, loot, descend, repeat.
//
// HP/XP/Gold/Levels, bump combat, potions, stairs, a boss every 5th floor.
#include "Multitool.h"

#define DG_W    25
#define DG_H    10
#define DG_CELL 5
#define DG_OX   1
#define DG_OY   12
#define DG_MAXE 10

enum { C_NONE = 0, C_GOLD, C_POTION, C_STAIRS };

struct Enemy { int8_t x, y; int16_t hp, maxhp; uint8_t atk; bool alive, boss; };

static uint8_t dmap[DG_H][DG_W];       // 0 wall, 1 floor
static uint8_t content[DG_H][DG_W];   // C_*
static Enemy   en[DG_MAXE];
static int     nEn;

static int  px, py;
static int  hp, maxhp, atk, lvl, xp, xpnext, floorN;
static uint32_t gold;
static uint8_t s_state;                // 0 play, 1 dead
static char s_msg[22];
static uint32_t s_msgUntil;

static void msg(const char* m) { strncpy(s_msg, m, sizeof(s_msg) - 1); s_msg[sizeof(s_msg)-1]=0; s_msgUntil = millis() + 1400; }

static bool isFloor(int x, int y) { return x >= 0 && x < DG_W && y >= 0 && y < DG_H && dmap[y][x]; }
static bool enemyAt(int x, int y, int* idx = nullptr) {
  for (int i = 0; i < nEn; i++) if (en[i].alive && en[i].x == x && en[i].y == y) { if (idx) *idx = i; return true; }
  return false;
}
static void randFloor(int& rx, int& ry) {
  for (int t = 0; t < 400; t++) {
    int x = rand() % DG_W, y = rand() % DG_H;
    if (dmap[y][x] && !(x == px && y == py) && !enemyAt(x, y) && content[y][x] == C_NONE) { rx = x; ry = y; return; }
  }
  rx = px; ry = py;
}

static void genFloor() {
  memset(dmap, 0, sizeof(dmap));
  memset(content, 0, sizeof(content));
  int cx = DG_W / 2, cy = DG_H / 2; dmap[cy][cx] = 1;
  int carved = 1, target = DG_W * DG_H * 55 / 100, wx = cx, wy = cy, guard = 0;
  while (carved < target && guard++ < 20000) {
    int d = rand() % 4;
    int nx = wx + (d == 0 ? 1 : d == 1 ? -1 : 0);
    int ny = wy + (d == 2 ? 1 : d == 3 ? -1 : 0);
    if (nx < 1 || nx >= DG_W - 1 || ny < 1 || ny >= DG_H - 1) continue;
    wx = nx; wy = ny;
    if (!dmap[wy][wx]) { dmap[wy][wx] = 1; carved++; }
  }
  // player
  px = cx; py = cy;
  for (int t = 0; t < 400 && !dmap[py][px]; t++) { px = rand() % DG_W; py = rand() % DG_H; }
  // stairs (far)
  int sxx, syy;
  for (int t = 0; t < 60; t++) { randFloor(sxx, syy); if (abs(sxx - px) + abs(syy - py) > (DG_W / 2)) break; }
  content[syy][sxx] = C_STAIRS;
  // items
  int items = 2 + rand() % 3;
  for (int i = 0; i < items; i++) { int x, y; randFloor(x, y); content[y][x] = (rand() % 3 == 0) ? C_POTION : C_GOLD; }
  // enemies
  nEn = 0;
  bool bossFloor = (floorN % 5 == 0);
  int count = bossFloor ? 3 : (2 + floorN); if (count > DG_MAXE) count = DG_MAXE;
  for (int i = 0; i < count; i++) {
    int x, y; randFloor(x, y);
    Enemy& e = en[nEn++];
    e.x = x; e.y = y; e.alive = true; e.boss = false;
    e.maxhp = 6 + floorN * 2; e.atk = 2 + floorN;
    if (bossFloor && i == 0) { e.boss = true; e.maxhp = 30 + floorN * 8; e.atk = 4 + floorN * 2; }
    e.hp = e.maxhp;
  }
}
static void dungeonReset() {
  floorN = 1; hp = maxhp = 24; atk = 4; lvl = 1; xp = 0; xpnext = 12; gold = 0;
  s_state = 0; s_msg[0] = 0;
  genFloor();
  msg("Enter the dungeon!");
  gBase(NPX_SOLID, strip.Color(80, 40, 0), 4);
}
void dungeonEnter() { srand(millis()); dungeonReset(); }

static void gainXp(int amount) {
  xp += amount;
  if (xp >= xpnext) {
    xp -= xpnext; lvl++; xpnext = lvl * 12;
    maxhp += 6; hp = maxhp; atk += 2;
    msg("LEVEL UP!");
    gSfx(2093, 60); gHap(30, 6); gFlash(strip.Color(0, 200, 40), 2, 110);
  }
}
static void hitEnemy(int i) {
  Enemy& e = en[i];
  int dmg = atk + rand() % 3;
  e.hp -= dmg;
  gSfx(1500, 12); gHap(10, 4);
  if (e.hp <= 0) {
    e.alive = false;
    uint32_t g = floorN + rand() % 5 + (e.boss ? 20 + floorN * 4 : 0);
    gold += g;
    gainXp(e.maxhp / 2 + floorN);
    msg(e.boss ? "BOSS SLAIN!" : "Enemy down");
    if (e.boss) { gFlash(strip.Color(200, 160, 0), 3, 120); }
  }
}
static void enemiesAct() {
  for (int i = 0; i < nEn; i++) {
    Enemy& e = en[i];
    if (!e.alive) continue;
    if (abs(e.x - px) + abs(e.y - py) == 1) {          // adjacent -> attack
      int dmg = e.atk + rand() % 3;
      hp -= dmg;
      gSfx(300, 18); gHap(14, 5);
      if (hp <= 0) { hp = 0; s_state = 1; gLose(); gameSubmitScore(floorN); return; }
    } else {                                            // step toward player
      int dx = (px > e.x) - (px < e.x), dy = (py > e.y) - (py < e.y);
      if (dx && isFloor(e.x + dx, e.y) && !enemyAt(e.x + dx, e.y) && !(e.x + dx == px && e.y == py)) e.x += dx;
      else if (dy && isFloor(e.x, e.y + dy) && !enemyAt(e.x, e.y + dy) && !(e.x == px && e.y + dy == py)) e.y += dy;
    }
  }
}
static void descend() {
  floorN++;
  if (hp < maxhp) hp += 2 + maxhp / 10;
  if (hp > maxhp) hp = maxhp;
  genFloor();
  char m[22]; snprintf(m, sizeof(m), "Floor %d%s", floorN, (floorN % 5 == 0) ? " - BOSS!" : "");
  msg(m);
  gSfx(1200, 40); gFlash(strip.Color(0, 80, 160), 1, 150);
}
static void tryMove(int dx, int dy) {
  int nx = px + dx, ny = py + dy;
  if (!isFloor(nx, ny)) return;                         // wall: no turn spent
  int ei;
  if (enemyAt(nx, ny, &ei)) { hitEnemy(ei); enemiesAct(); return; }
  px = nx; py = ny;
  if (content[ny][nx] == C_GOLD)   { gold += floorN * 2 + rand() % 6; content[ny][nx] = C_NONE; gSfx(2600, 14); }
  else if (content[ny][nx] == C_POTION) { int h = 8 + maxhp / 4; hp += h; if (hp > maxhp) hp = maxhp; content[ny][nx] = C_NONE; msg("Potion +HP"); gSfx(2200, 30); }
  else if (content[ny][nx] == C_STAIRS) { descend(); return; }  // no enemy turn on descend
  enemiesAct();
}

void dungeonLoop() {
  if (s_state == 0) {
    if (btnLong(BTN_SELECT)) { gameSubmitScore(floorN); gameReturnToHub(); return; }
    if (btnRepeat(BTN_UP))    tryMove(0, -1);
    else if (btnRepeat(BTN_DOWN))  tryMove(0, 1);
    else if (btnRepeat(BTN_LEFT))  tryMove(-1, 0);
    else if (btnRepeat(BTN_RIGHT)) tryMove(1, 0);
  } else {
    if (btnTap(BTN_SELECT)) { dungeonReset(); return; }
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { gameReturnToHub(); return; }
  }

  // ---- draw
  u8g2.clearBuffer();
  u8g2.setFont(F_TINY);
  char l[20]; snprintf(l, sizeof(l), "HP%d/%d L%d", hp, maxhp, lvl); u8g2.drawStr(1, 6, l);
  char rr[16]; snprintf(rr, sizeof(rr), "F%d $%lu", floorN, (unsigned long)gold);
  u8g2.drawStr(126 - u8g2.getStrWidth(rr), 6, rr);
  u8g2.drawHLine(0, 8, 128);

  for (int y = 0; y < DG_H; y++)
    for (int x = 0; x < DG_W; x++)
      if (!dmap[y][x]) u8g2.drawBox(DG_OX + x * DG_CELL, DG_OY + y * DG_CELL, DG_CELL, DG_CELL);

  for (int y = 0; y < DG_H; y++)
    for (int x = 0; x < DG_W; x++) {
      const char* g = nullptr;
      if (content[y][x] == C_GOLD) g = "$";
      else if (content[y][x] == C_POTION) g = "!";
      else if (content[y][x] == C_STAIRS) g = ">";
      if (g) u8g2.drawStr(DG_OX + x * DG_CELL, DG_OY + y * DG_CELL + 5, g);
    }
  for (int i = 0; i < nEn; i++)
    if (en[i].alive) u8g2.drawStr(DG_OX + en[i].x * DG_CELL, DG_OY + en[i].y * DG_CELL + 5, en[i].boss ? "B" : "e");
  u8g2.drawStr(DG_OX + px * DG_CELL, DG_OY + py * DG_CELL + 5, "@");

  if (s_msg[0] && millis() < s_msgUntil) {
    int w = u8g2.getStrWidth(s_msg) + 4;
    u8g2.setDrawColor(0); u8g2.drawBox(64 - w / 2, 56, w, 8); u8g2.setDrawColor(1);
    u8g2.drawStr(64 - u8g2.getStrWidth(s_msg) / 2, 62, s_msg);
  }
  if (s_state == 1) {
    char sc[18]; snprintf(sc, sizeof(sc), "Floor %d  $%lu", floorN, (unsigned long)gold);
    gfxCenterPanel("YOU DIED", sc, floorN >= (int)hiscoreGet("hsDungeon") ? "DEEPEST YET!" : "", "SEL=retry hold=quit");
  }
  gfxFlush();
}
