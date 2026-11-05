// "Spud Defense": tower defense for a 128x64 screen
//
// Bugs march along the path towards your pantry. Build towers beside the
// path, upgrade them, survive 20 waves. Four gardens, each unlocks the next;
// clear one with fewer leaks for more stars.
//
// Towers    Peeler   fast single shots, cheap
//           Masher   slow, heavy, splash damage
//           Chiller  light damage, slows whatever it hits
// Controls  D-pad move the cursor    SELECT build / upgrade / sell
//           hold SELECT  pause: send next wave early, 2x speed, quit
// Saves     stars and best wave per garden ("gDefense").
#include "Multitool.h"
#include "GameGfx.h"

#define GW 16
#define GH 7
#define OY 8                                   // playfield starts below the HUD
#define NMAP 4
#define WAVES 20

// ---- maps: waypoints in tile coordinates (may start/end just off-screen)
struct MapDef { const char* name; uint8_t n; int8_t pt[10][2]; uint32_t seed; };
static const MapDef MAPS[NMAP] = {
  { "Meadow",  8, { {-1,1},{3,1},{3,4},{8,4},{8,2},{12,2},{12,5},{16,5} }, 11 },
  { "Garden",  7, { {7,-1},{7,2},{2,2},{2,5},{12,5},{12,1},{16,1} }, 23 },
  { "Zigzag",  8, { {-1,0},{13,0},{13,2},{2,2},{2,4},{13,4},{13,6},{-1,6} }, 37 },
  { "Spiral",  8, { {-1,0},{14,0},{14,6},{1,6},{1,2},{11,2},{11,4},{5,4} }, 41 },
};

// ---- towers
enum : uint8_t { T_NONE = 0, T_PEEL, T_MASH, T_CHILL };
static const char* const T_NAME[4] = { "", "Peeler", "Masher", "Chiller" };
static const uint16_t T_COST[4] = { 0, 25, 40, 35 };
static const uint8_t T_DMG[4][3]   = { {0,0,0}, {2,3,5}, {4,7,11}, {1,1,2} };
static const uint8_t T_RATE[4][3]  = { {0,0,0}, {12,10,8}, {32,28,24}, {20,18,15} };
static const uint8_t T_RANGE[4][3] = { {0,0,0}, {18,20,23}, {16,17,19}, {16,18,20} };
static const float   T_SLOW[3] = { 0.55f, 0.45f, 0.35f };
static uint16_t upCost(uint8_t t, uint8_t lvl) { return lvl == 0 ? T_COST[t] * 6 / 5 : T_COST[t] * 9 / 5; }

static const uint8_t SP_PEEL[8]  = { R8("...XX..."), R8("..XXXX.."), R8("..X..X.."), R8(".XXXXXX."),
                                     R8(".X.XX.X."), R8(".XXXXXX."), R8("XX....XX"), R8("XXXXXXXX") };
static const uint8_t SP_MASH[8]  = { R8("XXXXXXXX"), R8("XXXXXXXX"), R8("...XX..."), R8("...XX..."),
                                     R8("..XXXX.."), R8(".X....X."), R8("X.XXXX.X"), R8("XXXXXXXX") };
static const uint8_t SP_CHILL[8] = { R8("X..X..X."), R8(".X.X.X.."), R8("..XXX..."), R8("XXX.XXX."),
                                     R8("..XXX..."), R8(".X.X.X.."), R8("X..X..X."), R8("........") };
static const uint8_t SP_TREE[8]  = { R8("..XXX..."), R8(".XX.XX.."), R8("XX.X.XX."), R8(".XXXXX.."),
                                     R8("..XXX..."), R8("...X...."), R8("...X...."), R8("..XXX...") };
static const uint8_t SP_HOUSE[8] = { R8("...XX..."), R8("..XXXX.."), R8(".XXXXXX."), R8("XXXXXXXX"),
                                     R8(".X....X."), R8(".X.XX.X."), R8(".X.XX.X."), R8(".XXXXXX.") };
static const uint8_t SP_BEETLE[6]= { R8("X....X"), R8(".XXXX."), R8("XX..XX"), R8("XXXXXX"), R8(".XXXX."), R8("X....X") };
static const uint8_t SP_FLY[6]   = { R8("XX..XX"), R8("X.XX.X"), R8(".XXXX."), R8("..XX.."), R8(".X..X."), R8("......") };
static const uint8_t SP_MOLE[6]  = { R8(".XXXX."), R8("XXXXXX"), R8("X.XX.X"), R8("XXXXXX"), R8("XX..XX"), R8(".XXXX.") };
static const uint8_t SP_BOSSB[8] = { R8("X......X"), R8(".X.XX.X."), R8("..XXXX.."), R8(".XX..XX."),
                                     R8("XXXXXXXX"), R8("X.XXXX.X"), R8(".XXXXXX."), R8("X.X..X.X") };

// ---- state
struct Tower { uint8_t type, lvl; int cd; uint16_t spent; };
static Tower  grid[GW][GH];
static uint8_t cell[GW][GH];                     // 0 free, 1 path, 2 tree
static float  segLen[10], pathLen;
static int    mapId;

enum : uint8_t { B_BEETLE = 1, B_FLY, B_MOLE, B_BOSS };
struct Bug { uint8_t type; bool alive; float d, hp, maxHp, speed; int slowT; float slowF; uint8_t gold; };
#define NBUG 40
static Bug bugs[NBUG];
struct Bolt { bool on; uint8_t type, lvl; float x, y; int target; };
#define NBOLT 24
static Bolt bolts[NBOLT];
struct Boom { float x, y; int t, r; };
#define NBOOM 6
static Boom booms[NBOOM];

static int lives, gold, wave, toSpawn, spawnT, spawnIdx, countdown, stateT;
static bool waveOn, fast;
static int cx = 5, cy = 3;                       // cursor
static uint8_t state;                            // 0 map select, 1 play, 2 build menu, 3 tower menu, 4 lost, 5 won
static int menuSel, selMap;
static PauseMenu dPause;
static GRng rng(1);

struct DefSave { uint8_t unlocked; uint8_t stars[NMAP]; uint8_t bestWave[NMAP]; };
static DefSave sv;
static void loadSave() { if (!gameLoad("gDefense", sv)) { memset(&sv, 0, sizeof(sv)); sv.unlocked = 1; } }
static int totalStars() { int n = 0; for (int i = 0; i < NMAP; i++) n += sv.stars[i]; return n; }

// ---- path
static inline float tcx(int tx) { return tx * 8 + 4; }
static inline float tcy(int ty) { return OY + ty * 8 + 4; }
static void pathPos(float d, float& x, float& y) {
  const MapDef& m = MAPS[mapId];
  for (int i = 0; i < m.n - 1; i++) {
    if (d <= segLen[i] || i == m.n - 2) {
      float t = segLen[i] > 0 ? min(1.0f, d / segLen[i]) : 0;
      x = tcx(m.pt[i][0]) + (tcx(m.pt[i + 1][0]) - tcx(m.pt[i][0])) * t;
      y = tcy(m.pt[i][1]) + (tcy(m.pt[i + 1][1]) - tcy(m.pt[i][1])) * t;
      return;
    }
    d -= segLen[i];
  }
  x = y = 0;
}
static void buildMap() {
  memset(cell, 0, sizeof(cell)); memset(grid, 0, sizeof(grid));
  const MapDef& m = MAPS[mapId];
  pathLen = 0;
  for (int i = 0; i < m.n - 1; i++) {
    int x0 = m.pt[i][0], y0 = m.pt[i][1], x1 = m.pt[i + 1][0], y1 = m.pt[i + 1][1];
    segLen[i] = (abs(x1 - x0) + abs(y1 - y0)) * 8.0f; pathLen += segLen[i];
    int sx = (x1 > x0) - (x1 < x0), sy = (y1 > y0) - (y1 < y0);
    for (int x = x0, y = y0;; x += sx, y += sy) {
      if (x >= 0 && y >= 0 && x < GW && y < GH) cell[x][y] = 1;
      if (x == x1 && y == y1) break;
    }
  }
  GRng r(m.seed);                                // a few trees, same every time
  for (int k = 0; k < 9; k++) {
    int x = r.range(0, GW - 1), y = r.range(0, GH - 1);
    if (!cell[x][y]) cell[x][y] = 2;
  }
}

// ---- waves
static float hpMul() { float w = wave - 1; return 1 + 0.25f * w + 0.02f * w * w; }
static void startWave() {
  wave++; waveOn = true;
  toSpawn = 6 + wave + (wave >= 10 ? wave / 2 : 0);
  spawnIdx = 0; spawnT = 0;
  gSfx(330, 120); gHap(40, 5);
}
static void spawnBug() {
  uint8_t type = B_BEETLE;
  bool bossWave = wave % 5 == 0;
  if (bossWave && spawnIdx == toSpawn / 2) type = B_BOSS;
  else {
    int r = rng.range(0, 99);
    if (wave >= 6 && r < 20 + wave) type = B_MOLE;
    else if (wave >= 3 && r < 55) type = B_FLY;
  }
  for (auto& b : bugs) if (!b.alive) {
    b = {}; b.type = type; b.alive = true; b.d = 0;
    float base = type == B_BEETLE ? 6 : type == B_FLY ? 3 : type == B_MOLE ? 18 : 70 + wave * 6;
    b.hp = b.maxHp = base * hpMul();
    b.speed = type == B_BEETLE ? 0.45f : type == B_FLY ? 0.85f : type == B_MOLE ? 0.3f : 0.26f;
    b.gold = type == B_BEETLE ? 3 : type == B_FLY ? 2 : type == B_MOLE ? 6 : 30;
    b.slowF = 1;
    break;
  }
  spawnIdx++;
  spawnT = type == B_FLY ? 14 : type == B_BOSS ? 40 : 24;
}

static void startMap() {
  buildMap();
  memset(bugs, 0, sizeof(bugs)); memset(bolts, 0, sizeof(bolts)); memset(booms, 0, sizeof(booms));
  lives = 10; gold = 70; wave = 0; waveOn = false; countdown = 30 * 12; fast = false;
  cx = 5; cy = 3; state = 1; dPause.open = false;
  rng = GRng(millis() | 1);
  gBase(NPX_SOLID, 0x206000, 3);
}

static void hitBug(int i, float dmg, uint8_t ttype, uint8_t lvl) {
  Bug& b = bugs[i];
  if (!b.alive) return;
  b.hp -= dmg;
  if (ttype == T_CHILL) { b.slowT = 45; b.slowF = min(b.slowF, T_SLOW[lvl]); }
  if (b.hp <= 0) {
    b.alive = false; gold += b.gold;
    gSfx(b.type == B_BOSS ? 220 : 1800, b.type == B_BOSS ? 200 : 10);
    if (b.type == B_BOSS) { gHap(120, 8); gFlash(0xFFC000, 3, 120); }
  }
}

static void simStep() {
  // spawning
  if (waveOn) {
    if (toSpawn > spawnIdx) { if (--spawnT <= 0) spawnBug(); }
    else {
      bool any = false; for (auto& b : bugs) if (b.alive) { any = true; break; }
      if (!any) {
        waveOn = false; gold += 10 + wave;
        if (wave >= WAVES) {
          state = 5; stateT = 0;
          int stars = lives >= 10 ? 3 : lives >= 5 ? 2 : 1;
          sv.stars[mapId] = max<uint8_t>(sv.stars[mapId], stars);
          sv.bestWave[mapId] = WAVES;
          if (sv.unlocked < NMAP && mapId + 1 >= sv.unlocked) sv.unlocked = mapId + 2;
          gameSave("gDefense", sv); gameSubmitScore(totalStars());
          gWin(); return;
        }
        countdown = 30 * 10;
        static const ToneStep t[] = { { 784, 60 }, { 1047, 120 } };
        if (S.gameSfx) buzzSeq(t, 2);
      }
    }
  } else if (--countdown <= 0) startWave();

  // bugs walk
  for (auto& b : bugs) {
    if (!b.alive) continue;
    float sp = b.speed * (b.slowT > 0 ? b.slowF : 1);
    if (b.slowT > 0 && --b.slowT == 0) b.slowF = 1;
    b.d += sp;
    if (b.d >= pathLen) {
      b.alive = false;
      lives -= b.type == B_BOSS ? 5 : 1;
      gSfx(160, 120); gHap(90, 8);
      if (lives <= 0) {
        lives = 0; state = 4; stateT = 0;
        if (wave > sv.bestWave[mapId]) sv.bestWave[mapId] = wave;
        gameSave("gDefense", sv); gLose(); return;
      }
    }
  }
  // towers aim at the bug furthest along the path inside their range
  for (int x = 0; x < GW; x++) for (int y = 0; y < GH; y++) {
    Tower& t = grid[x][y];
    if (!t.type) continue;
    if (t.cd > 0) { t.cd--; continue; }
    float r = T_RANGE[t.type][t.lvl], best = -1; int bi = -1;
    for (int i = 0; i < NBUG; i++) {
      const Bug& b = bugs[i];
      if (!b.alive) continue;
      float bx, by; pathPos(b.d, bx, by);
      float dx = bx - tcx(x), dy = by - tcy(y);
      if (dx * dx + dy * dy <= r * r && b.d > best) { best = b.d; bi = i; }
    }
    if (bi < 0) continue;
    for (auto& bo : bolts) if (!bo.on) { bo = { true, t.type, t.lvl, tcx(x), tcy(y) - 2, bi }; break; }
    t.cd = T_RATE[t.type][t.lvl];
  }
  // bolts home in
  for (auto& bo : bolts) {
    if (!bo.on) continue;
    Bug& b = bugs[bo.target];
    if (!b.alive) { bo.on = false; continue; }
    float bx, by; pathPos(b.d, bx, by);
    float dx = bx - bo.x, dy = by - bo.y, d = sqrtf(dx * dx + dy * dy);
    float v = bo.type == T_MASH ? 2.2f : 3.2f;
    if (d <= v) {
      bo.on = false;
      float dmg = T_DMG[bo.type][bo.lvl];
      if (bo.type == T_MASH) {
        for (int i = 0; i < NBUG; i++) {
          if (!bugs[i].alive) continue;
          float x2, y2; pathPos(bugs[i].d, x2, y2);
          if ((x2 - bx) * (x2 - bx) + (y2 - by) * (y2 - by) <= 100) hitBug(i, i == bo.target ? dmg : dmg * 0.6f, T_MASH, bo.lvl);
        }
        for (auto& bm : booms) if (bm.t <= 0) { bm = { bx, by, 6, 10 }; break; }
      } else hitBug(bo.target, dmg, bo.type, bo.lvl);
    } else { bo.x += dx / d * v; bo.y += dy / d * v; }
  }
  for (auto& bm : booms) if (bm.t > 0) bm.t--;
}

// ---- drawing
static void drawField() {
  for (int x = 0; x < GW; x++) for (int y = 0; y < GH; y++) {
    int sx = x * 8, sy = OY + y * 8;
    if (cell[x][y] == 1) {                                     // road: edges where it meets grass
      auto road = [&](int nx, int ny) { return nx < 0 || ny < 0 || nx >= GW || ny >= GH || cell[nx][ny] == 1; };
      if (!road(x, y - 1)) u8g2.drawHLine(sx, sy, 8);
      if (!road(x, y + 1)) u8g2.drawHLine(sx, sy + 7, 8);
      if (!road(x - 1, y)) u8g2.drawVLine(sx, sy, 8);
      if (!road(x + 1, y)) u8g2.drawVLine(sx + 7, sy, 8);
      u8g2.drawPixel(sx + ((x * 3 + y) % 5) + 1, sy + ((x + y * 5) % 4) + 2);   // a little gravel
    } else if (cell[x][y] == 2) spr8(sx, sy, SP_TREE, 8);
    const Tower& t = grid[x][y];
    if (t.type) {
      spr8(sx, sy, t.type == T_PEEL ? SP_PEEL : t.type == T_MASH ? SP_MASH : SP_CHILL, 8);
      for (int k = 0; k < t.lvl; k++) { u8g2.setDrawColor(0); u8g2.drawPixel(sx + 3 + k * 2, sy + 7); u8g2.setDrawColor(1); }
    }
  }
  // pantry at the end of the path
  const MapDef& m = MAPS[mapId];
  int ex = m.pt[m.n - 1][0], ey = m.pt[m.n - 1][1];
  if (ex >= 0 && ex < GW && ey >= 0 && ey < GH) { u8g2.setDrawColor(0); u8g2.drawBox(ex * 8, OY + ey * 8, 8, 8); u8g2.setDrawColor(1); spr8(ex * 8, OY + ey * 8, SP_HOUSE, 8); }
  for (const auto& b : bugs) {
    if (!b.alive) continue;
    float x, y; pathPos(b.d, x, y);
    int sx = (int)x, sy = (int)y;
    if (b.type == B_BOSS) spr8o(sx - 4, sy - 4, SP_BOSSB, 8);
    else {
      const uint8_t* s = b.type == B_BEETLE ? SP_BEETLE : b.type == B_FLY ? SP_FLY : SP_MOLE;
      int fy = (b.type == B_FLY && ((millis() / 120) & 1)) ? -1 : 0;
      u8g2.setDrawColor(0); u8g2.drawBox(sx - 3, sy - 3 + fy, 6, 6); u8g2.setDrawColor(1);
      spr8(sx - 3, sy - 3 + fy, s, 6);
    }
    if (b.hp < b.maxHp) { int w = (int)(7 * b.hp / b.maxHp) + 1; u8g2.drawHLine(sx - 4, sy - (b.type == B_BOSS ? 7 : 6), w); }
    if (b.slowT > 0 && ((millis() / 200) & 1)) u8g2.drawPixel(sx + 4, sy - 4);
  }
  for (const auto& bo : bolts) {
    if (!bo.on) continue;
    if (bo.type == T_MASH) u8g2.drawBox((int)bo.x - 1, (int)bo.y - 1, 3, 3);
    else if (bo.type == T_CHILL) u8g2.drawCircle((int)bo.x, (int)bo.y, 1);
    else u8g2.drawPixel((int)bo.x, (int)bo.y);
  }
  for (const auto& bm : booms) if (bm.t > 0) u8g2.drawCircle((int)bm.x, (int)bm.y, bm.r - bm.t);
}

static void drawHud() {
  u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 128, OY); u8g2.setDrawColor(1);
  u8g2.setFont(F_TINY);
  char b[24];
  // heart + lives
  u8g2.drawPixel(1, 1); u8g2.drawPixel(2, 1); u8g2.drawPixel(4, 1); u8g2.drawPixel(5, 1);
  u8g2.drawHLine(1, 2, 5); u8g2.drawHLine(2, 3, 3); u8g2.drawPixel(3, 4);
  snprintf(b, sizeof(b), "%d", lives); u8g2.drawStr(8, 6, b);
  snprintf(b, sizeof(b), "$%d", gold); u8g2.drawStr(22, 6, b);
  snprintf(b, sizeof(b), "W%d/%d", wave, WAVES); u8g2.drawStr(50, 6, b);
  if (!waveOn && state == 1) { snprintf(b, sizeof(b), "next %d", countdown / 30 + 1); u8g2.drawStr(127 - u8g2.getStrWidth(b), 6, b); }
  else if (fast) u8g2.drawStr(127 - u8g2.getStrWidth(">>"), 6, ">>");
}

static void drawCursor() {
  int sx = cx * 8, sy = OY + cy * 8;
  bool on = (millis() / 250) & 1;
  if (on) { u8g2.setDrawColor(2); u8g2.drawFrame(sx - 1, sy - 1, 10, 10); u8g2.setDrawColor(1); }
  else {                                                     // corner brackets
    u8g2.drawHLine(sx - 1, sy - 1, 3); u8g2.drawVLine(sx - 1, sy - 1, 3);
    u8g2.drawHLine(sx + 6, sy - 1, 3); u8g2.drawVLine(sx + 8, sy - 1, 3);
    u8g2.drawHLine(sx - 1, sy + 8, 3); u8g2.drawVLine(sx - 1, sy + 6, 3);
    u8g2.drawHLine(sx + 6, sy + 8, 3); u8g2.drawVLine(sx + 8, sy + 6, 3);
  }
  const Tower& t = grid[cx][cy];
  int r = 0;
  if (state == 2) r = T_RANGE[menuSel + 1][0];
  else if (t.type) r = T_RANGE[t.type][t.lvl];
  if (r) for (int a = 0; a < 32; a += 1) if ((a + (millis() / 100)) % 2 == 0) {
    float an = a * 6.2832f / 32;
    u8g2.drawPixel(sx + 4 + (int)(cosf(an) * r), sy + 4 + (int)(sinf(an) * r));
  }
}

static void panel(int w, int h, int& x, int& y) {
  x = cx < 8 ? 126 - w : 2; y = OY + 2;
  u8g2.setDrawColor(0); u8g2.drawBox(x, y, w, h); u8g2.setDrawColor(1); u8g2.drawRFrame(x, y, w, h, 2);
}

static void drawBuildMenu() {
  int x, y; panel(62, 40, x, y);
  u8g2.setFont(F_TINY);
  for (int i = 0; i < 3; i++) {
    int ry = y + 9 + i * 8;
    bool afford = gold >= T_COST[i + 1];
    if (i == menuSel) { u8g2.drawBox(x + 2, ry - 6, 58, 8); u8g2.setDrawColor(0); }
    u8g2.drawStr(x + 4, ry, T_NAME[i + 1]);
    char b[8]; snprintf(b, sizeof(b), "%s$%u", afford ? "" : "!", T_COST[i + 1]); u8g2.drawStr(x + 58 - u8g2.getStrWidth(b), ry, b);
    u8g2.setDrawColor(1);
  }
  static const char* const DESC[3] = { "fast shots", "splash hits", "slows bugs" };
  u8g2.drawStr(x + 4, y + 36, DESC[menuSel]);
}

static void drawTowerMenu() {
  const Tower& t = grid[cx][cy];
  int x, y; panel(62, 40, x, y);
  u8g2.setFont(F_TINY);
  char b[24]; snprintf(b, sizeof(b), "%s L%d", T_NAME[t.type], t.lvl + 1); u8g2.drawStr(x + 4, y + 8, b);
  u8g2.drawHLine(x + 2, y + 10, 58);
  const char* items[3]; char up[16], sell[16];
  if (t.lvl < 2) snprintf(up, sizeof(up), "Upgrade $%u", upCost(t.type, t.lvl)); else snprintf(up, sizeof(up), "Max level");
  snprintf(sell, sizeof(sell), "Sell +$%u", t.spent * 3 / 5);
  items[0] = up; items[1] = sell; items[2] = "Cancel";
  for (int i = 0; i < 3; i++) {
    int ry = y + 18 + i * 8;
    if (i == menuSel) { u8g2.drawBox(x + 2, ry - 6, 58, 8); u8g2.setDrawColor(0); }
    u8g2.drawStr(x + 4, ry, items[i]);
    u8g2.setDrawColor(1);
  }
}

// ---- screens
static void selectLoop() {
  if (btnLong(BTN_SELECT)) { gameReturnToHub(); return; }
  if (btnRepeat(BTN_DOWN)) { selMap = (selMap + 1) % NMAP; gClick(); }
  if (btnRepeat(BTN_UP))   { selMap = (selMap + NMAP - 1) % NMAP; gClick(); }
  bool locked = selMap >= sv.unlocked;
  if (btnTap(BTN_SELECT)) { if (locked) gSfx(200, 60); else { mapId = selMap; startMap(); return; } }
  u8g2.clearBuffer();
  u8g2.setFont(F_UI); u8g2.drawStr(2, 10, "Spud Defense");
  u8g2.setFont(F_TINY);
  char b[24]; snprintf(b, sizeof(b), "*%d/%d", totalStars(), NMAP * 3); u8g2.drawStr(126 - u8g2.getStrWidth(b), 9, b);
  u8g2.drawHLine(0, 12, 128);
  for (int i = 0; i < NMAP; i++) {
    int y = 22 + i * 10;
    bool lk = i >= sv.unlocked;
    if (i == selMap) { u8g2.drawBox(0, y - 8, 128, 10); u8g2.setDrawColor(0); }
    u8g2.setFont(F_SMALL); u8g2.drawStr(3, y, lk ? "locked" : MAPS[i].name);
    if (!lk) {
      for (int s = 0; s < 3; s++) { int sx = 70 + s * 7; if (s < sv.stars[i]) u8g2.drawBox(sx, y - 6, 5, 5); else u8g2.drawFrame(sx, y - 6, 5, 5); }
      u8g2.setFont(F_TINY);
      if (sv.bestWave[i]) { snprintf(b, sizeof(b), "w%d", sv.bestWave[i]); u8g2.drawStr(125 - u8g2.getStrWidth(b), y - 1, b); }
    }
    u8g2.setDrawColor(1);
  }
  u8g2.setFont(F_TINY); u8g2.drawStr(2, 63, "SEL play   hold SEL exit");
  gfxFlush();
}

void defenseEnter() { loadSave(); state = 0; selMap = min<int>(sv.unlocked - 1, NMAP - 1); }

void defenseLoop() {
  if (state == 0) { selectLoop(); return; }

  static const char* EXTRA[2];
  EXTRA[0] = waveOn ? "Speed x1/x2" : "Send wave now";
  EXTRA[1] = waveOn ? "Restart garden" : "Speed x1/x2";
  if ((state == 1) && btnLong(BTN_SELECT) && !dPause.open) { dPause.open = true; dPause.sel = 0; gClick(); }
  bool wasOpen = dPause.open;
  int pr = dPause.update(EXTRA, 2);
  if (pr == 2) { state = 0; return; }
  if (pr == 3) { if (waveOn) fast = !fast; else { gold += countdown / 30; startWave(); } }
  if (pr == 4) { if (waveOn) { startMap(); return; } fast = !fast; }

  if (!dPause.open && !wasOpen) {
    if (state == 1) {
      if (btnRepeat(BTN_LEFT)  && cx > 0)      { cx--; gClick(); }
      if (btnRepeat(BTN_RIGHT) && cx < GW - 1) { cx++; gClick(); }
      if (btnRepeat(BTN_UP)    && cy > 0)      { cy--; gClick(); }
      if (btnRepeat(BTN_DOWN)  && cy < GH - 1) { cy++; gClick(); }
      if (btnTap(BTN_SELECT)) {
        if (grid[cx][cy].type) { state = 3; menuSel = grid[cx][cy].lvl < 2 ? 0 : 1; gClick(); }
        else if (cell[cx][cy] == 0) { state = 2; menuSel = 0; gClick(); }
        else gSfx(200, 40);
      }
    } else if (state == 2) {                                   // build menu
      if (btnRepeat(BTN_DOWN)) { menuSel = (menuSel + 1) % 3; gClick(); }
      if (btnRepeat(BTN_UP))   { menuSel = (menuSel + 2) % 3; gClick(); }
      if (btnTap(BTN_LEFT) || btnTap(BTN_RIGHT)) { state = 1; gClick(); }
      if (btnTap(BTN_SELECT)) {
        uint8_t t = menuSel + 1;
        if (gold < T_COST[t]) gSfx(200, 60);
        else { gold -= T_COST[t]; grid[cx][cy] = { t, 0, 0, T_COST[t] }; state = 1; gSfx(1200, 40); gHap(25, 5); }
      }
    } else if (state == 3) {                                   // tower menu
      Tower& t = grid[cx][cy];
      if (btnRepeat(BTN_DOWN)) { menuSel = (menuSel + 1) % 3; gClick(); }
      if (btnRepeat(BTN_UP))   { menuSel = (menuSel + 2) % 3; gClick(); }
      if (btnTap(BTN_LEFT) || btnTap(BTN_RIGHT)) { state = 1; gClick(); }
      if (btnTap(BTN_SELECT)) {
        if (menuSel == 0) {
          uint16_t c = t.lvl < 2 ? upCost(t.type, t.lvl) : 0;
          if (!c || gold < c) gSfx(200, 60);
          else { gold -= c; t.spent += c; t.lvl++; gSfx(1600, 50); gHap(25, 5); if (t.lvl == 2) menuSel = 1; }
        } else if (menuSel == 1) { gold += t.spent * 3 / 5; t = {}; state = 1; gSfx(700, 50); }
        else state = 1;
      }
    } else if (state == 4 || state == 5) {
      stateT++;
      if (stateT > 20 && btnTap(BTN_SELECT)) { state = 0; return; }
    }
    if (state >= 1 && state <= 3) { simStep(); if (fast && state >= 1 && state <= 3) simStep(); }
  }

  u8g2.clearBuffer();
  drawField();
  drawHud();
  if (state >= 1 && state <= 3) drawCursor();
  if (state == 2) drawBuildMenu();
  if (state == 3) drawTowerMenu();
  if (state == 4 || state == 5) {
    u8g2.setDrawColor(0); u8g2.drawBox(16, 14, 96, 40); u8g2.setDrawColor(1); u8g2.drawRFrame(16, 14, 96, 40, 3);
    u8g2.setFont(F_MEDB);
    const char* t = state == 5 ? "SAFE!" : "OVERRUN";
    u8g2.drawStr(64 - u8g2.getStrWidth(t) / 2, 28, t);
    u8g2.setFont(F_SMALL);
    char b[28];
    if (state == 5) { int st = lives >= 10 ? 3 : lives >= 5 ? 2 : 1; snprintf(b, sizeof(b), "%d star%s, %d lives left", st, st > 1 ? "s" : "", lives); }
    else snprintf(b, sizeof(b), "fell on wave %d", wave);
    u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 39, b);
    u8g2.setFont(F_TINY); u8g2.drawStr(64 - u8g2.getStrWidth("SEL gardens") / 2, 49, "SEL gardens");
  }
  dPause.draw("Paused", EXTRA, 2);
  gfxFlush();
}
