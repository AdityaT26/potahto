// "Spud Knight": a small action roguelite
//
// Every run is a fresh dungeon. Each floor is a 5x5 grid of rooms; doors lock
// until the room is clear. Find the treasure chest, find the stairs, go deeper.
// Every 3rd floor ends in a boss. Coins you pick up are banked even when you
// die and buy permanent upgrades at the camp (Heart, Blade, Boots, Luck).
//
// Controls  D-pad move (diagonals if your buttons allow two at once)
//           SELECT swing the sword        hold SELECT  pause / map / quit
// Score     deepest floor reached.   Save "gKnight" (bank, upgrades, best).
#include "Multitool.h"
#include "GameGfx.h"

// ---- geometry
//  HUD y 0..7, walls 4 px, interior x 4..123, y 12..59 = 15 x 6 tiles of 8 px.
#define IX0 4
#define IY0 12
#define IX1 124
#define IY1 60
#define TW  15
#define TH  6
#define GRID 5
#define PSZ 7                      // player hitbox

// ---- room templates: '#' rock, 'o' pit. Rows 2-3 and columns 6-8 stay open
//      so every door connects to every other door.
static const char* const TMPL[][TH] = {
  { "...............", "...............", "...............", "...............", "...............", "..............." },
  { "...............", ".#..#.....#..#.", "...............", "...............", ".#..#.....#..#.", "..............." },
  { "##...........##", "#.............#", "...............", "...............", "#.............#", "##...........##" },
  { "...............", ".ooo.......ooo.", "...............", "...............", ".ooo.......ooo.", "..............." },
  { "....#.....#....", "....#.....#....", "...............", "...............", "....#.....#....", "....#.....#...." },
  { ".....#...#.....", "..#.........#..", "...............", "...............", "..#.........#..", ".....#...#....." },
  { "ooo.........ooo", "o.............o", "...............", "...............", "o.............o", "ooo.........ooo" },
  { "..#.........#..", "..#..ooooo..#..", "...............", "...............", "..#..ooooo..#..", "..#.........#.." },
};
#define NTMPL (int)(sizeof(TMPL) / sizeof(TMPL[0]))

// ---- sprites
static const uint8_t SK_SIDE[8] = { R8("..XXX..."), R8(".XXXXX.."), R8(".X.X.XX."), R8(".XXXXX.."),
                                    R8("XXXXXXX."), R8("X.XXX.X."), R8("..X.X..."), R8(".XX.XX..") };
static const uint8_t SK_DOWN[8] = { R8("..XXXX.."), R8(".XXXXXX."), R8(".X.XX.X."), R8(".XXXXXX."),
                                    R8("XXXXXXXX"), R8("X.XXXX.X"), R8("..X..X.."), R8(".XX..XX.") };
static const uint8_t SK_UP[8]   = { R8("..XXXX.."), R8(".XXXXXX."), R8(".XXXXXX."), R8(".XXXXXX."),
                                    R8("XXXXXXXX"), R8("X.XXXX.X"), R8("..X..X.."), R8(".XX..XX.") };
static const uint8_t SP_SLIME[8] = { R8("........"), R8("........"), R8("..XXXX.."), R8(".XXXXXX."),
                                     R8("XX.XX.XX"), R8("XXXXXXXX"), R8("XXXXXXXX"), R8(".XXXXXX.") };
static const uint8_t SP_SLIME2[8]= { R8("........"), R8("........"), R8("........"), R8("..XXXX.."),
                                     R8(".XX.XX.X"), R8("XXXXXXXX"), R8("XXXXXXXX"), R8("XXXXXXXX") };
static const uint8_t SP_BAT[8]   = { R8("........"), R8("X......X"), R8("XX.XX.XX"), R8("XXXXXXXX"),
                                     R8(".XX..XX."), R8("..XXXX.."), R8("........"), R8("........") };
static const uint8_t SP_BAT2[8]  = { R8("........"), R8("........"), R8("..X..X.."), R8(".XXXXXX."),
                                     R8("XXXXXXXX"), R8("X.XXXX.X"), R8("........"), R8("........") };
static const uint8_t SP_ARCH[8]  = { R8("..XXX..."), R8(".XXXXX.X"), R8(".X.X.X.X"), R8(".XXXXXXX"),
                                     R8("..XXX..X"), R8(".XXXXX.X"), R8("..X.X..X"), R8(".XX.XX..") };
static const uint8_t SP_GUARD[8] = { R8("X.XXXX.X"), R8("XXXXXXXX"), R8("X..XX..X"), R8("XXXXXXXX"),
                                     R8(".XXXXXX."), R8("XXXXXXXX"), R8(".XX..XX."), R8(".XX..XX.") };
static const uint8_t SP_ROCK[8]  = { R8("..XXXX.."), R8(".X..X.X."), R8("X..X...X"), R8("X.....XX"),
                                     R8("XX..X..X"), R8("X.X....X"), R8(".X...XX."), R8("..XXXX..") };
static const uint8_t SP_HEART[8] = { R8("........"), R8(".XX.XX.."), R8("XXXXXXX."), R8("XXXXXXX."),
                                     R8(".XXXXX.."), R8("..XXX..."), R8("...X...."), R8("........") };
static const uint8_t SP_CHEST[8] = { R8("........"), R8(".XXXXXX."), R8("X......X"), R8("XXXXXXXX"),
                                     R8("X..XX..X"), R8("X......X"), R8("XXXXXXXX"), R8("........") };
static const uint16_t SP_BOSS[16] = {
  R16("...X...XX...X..."), R16("...XX.XXXX.XX..."), R16("...XXXXXXXXXX..."), R16("....XXXXXXXX...."),
  R16("..XXXXXXXXXXXX.."), R16(".XXXXXXXXXXXXXX."), R16("XXX..XXXXXX..XXX"), R16("XXX..XXXXXX..XXX"),
  R16("XXXXXXXXXXXXXXXX"), R16("XXXXXXXXXXXXXXXX"), R16("XXX.XXXXXXXX.XXX"), R16("XXXX........XXXX"),
  R16(".XXXX.X..X.XXXX."), R16(".XXXXXXXXXXXXXX."), R16("..XXXXXXXXXXXX.."), R16("....XXXXXXXX....") };

// ---- state
enum : uint8_t { RT_NONE = 0, RT_START, RT_FIGHT, RT_TREASURE, RT_STAIRS, RT_BOSS };
struct Room { uint8_t type, tmpl; bool cleared, visited, looted; };
static Room rooms[GRID][GRID];
static int  rx, ry;                                  // current room

enum : uint8_t { M_SLIME = 1, M_BAT, M_ARCHER, M_GUARD, M_BOSS };
struct Mob { uint8_t type; bool alive; float x, y, vx, vy, kx, ky; int hp, maxHp, t, cd, st; uint8_t hitSwing; };
#define NMOB 8
static Mob mobs[NMOB];
struct Shot { bool on; float x, y, vx, vy; bool big; };
#define NSHOT 18
static Shot shots[NSHOT];
enum : uint8_t { D_COIN = 1, D_HEART };
struct Drop { uint8_t kind; float x, y; int t; uint8_t val; };
#define NDROP 8
static Drop drops[NDROP];
struct Spark { float x, y, vx, vy; int t; };
#define NSPARK 14
static Spark sparks[NSPARK];

static float px, py;
static int   face = 1;                               // 0 up, 1 right, 2 down, 3 left
static int   hp, maxHp, dmg, iframes, swing, swingCd, swingId;
static float speed, pkx, pky;
static int   floorN, runCoins;
static bool  chestHere; static float chestX, chestY;
static bool  stairsHere;
static int   stateT, shake;
static char  msg[24]; static int msgT;                // in-game banner (the system toast isn't drawn in games)
static void say(const char* m) { strncpy(msg, m, sizeof(msg) - 1); msg[sizeof(msg) - 1] = 0; msgT = 45; }
static uint8_t state;                                // 0 camp, 1 play, 2 floor intro, 3 dead, 4 map, 5 upgrades
static int   campSel, upSel;
static PauseMenu kPause;
static GRng  rng(1);

// ---- persistent
struct KnightSave { uint32_t bank; uint8_t up[4]; uint16_t best; uint16_t runs; };
static KnightSave sv;
static const char* const UP_NAME[4] = { "Heart", "Blade", "Boots", "Luck" };
static const char* const UP_DESC[4] = { "+1 max heart", "+1 sword damage", "move faster", "more drops" };
static const uint8_t  UP_MAX[4]  = { 3, 2, 2, 2 };
static const uint16_t UP_COST[4][3] = { { 30, 80, 160 }, { 60, 150, 0 }, { 40, 100, 0 }, { 40, 100, 0 } };
static void loadSave() { if (!gameLoad("gKnight", sv)) memset(&sv, 0, sizeof(sv)); }
static void save() { gameSave("gKnight", sv); }

// ---- tiles & collision
static inline char tileAt(int tx, int ty) {
  if (tx < 0 || tx >= TW || ty < 0 || ty >= TH) return '#';
  return TMPL[rooms[rx][ry].tmpl][ty][tx];
}
static bool hasDoor(int dir) {
  int nx = rx + (dir == 1) - (dir == 3), ny = ry + (dir == 2) - (dir == 0);
  return nx >= 0 && ny >= 0 && nx < GRID && ny < GRID && rooms[nx][ny].type != RT_NONE;
}
static bool doorsOpen() { return rooms[rx][ry].cleared; }
// flyer: bats pass over pits. player: may step into open door gaps.
static bool blocked(float x, float y, int w, int h, bool flyer, bool player) {
  bool out = x < IX0 || y < IY0 || x + w > IX1 || y + h > IY1;
  if (out) {
    if (!player || !doorsOpen()) return true;
    bool ok = false;
    if (y < IY0 && x >= 56 && x + w <= 72 && hasDoor(0)) ok = true;
    if (y + h > IY1 && x >= 56 && x + w <= 72 && hasDoor(2)) ok = true;
    if (x < IX0 && y >= 28 && y + h <= 44 && hasDoor(3)) ok = true;
    if (x + w > IX1 && y >= 28 && y + h <= 44 && hasDoor(1)) ok = true;
    if (!ok) return true;
  }
  int tx0 = (int)floorf((x - IX0) / 8), tx1 = (int)floorf((x + w - 0.01f - IX0) / 8);
  int ty0 = (int)floorf((y - IY0) / 8), ty1 = (int)floorf((y + h - 0.01f - IY0) / 8);
  for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
    if (tx < 0 || tx >= TW || ty < 0 || ty >= TH) continue;     // door gap
    char t = tileAt(tx, ty);
    if (t == '#' || (t == 'o' && !flyer)) return true;
  }
  return false;
}
static int mobSize(uint8_t t) { return t == M_BOSS ? 14 : 7; }

static void spark(float x, float y, int n) {
  for (int i = 0; i < NSPARK && n > 0; i++) if (sparks[i].t <= 0) {
    float a = rng.range(0, 628) / 100.0f, v = rng.range(6, 16) / 10.0f;
    sparks[i] = { x, y, cosf(a) * v, sinf(a) * v, rng.range(6, 12) }; n--;
  }
}

// ---- floor generation
static void buildFloor() {
  memset(rooms, 0, sizeof(rooms));
  int want = min(6 + floorN, 13);
  int x = 2, y = 2, made = 1;
  rooms[x][y] = { RT_START, 0, true, true, false };
  for (int guard = 0; made < want && guard < 400; guard++) {
    int d = rng.range(0, 3);
    int nx = x + (d == 1) - (d == 3), ny = y + (d == 2) - (d == 0);
    if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) continue;
    x = nx; y = ny;
    if (rooms[x][y].type == RT_NONE) { rooms[x][y] = { RT_FIGHT, (uint8_t)rng.range(0, NTMPL - 1), false, false, false }; made++; }
  }
  // BFS distance from start -> stairs/boss = farthest, treasure = another dead end
  int dist[GRID][GRID]; for (auto& c : dist) for (int& v : c) v = -1;
  int qx[25], qy[25], qh = 0, qt = 0; qx[qt] = 2; qy[qt++] = 2; dist[2][2] = 0;
  int fx = 2, fy = 2;
  while (qh < qt) {
    int cx = qx[qh], cy = qy[qh++];
    if (dist[cx][cy] > dist[fx][fy]) { fx = cx; fy = cy; }
    for (int d = 0; d < 4; d++) {
      int nx = cx + (d == 1) - (d == 3), ny = cy + (d == 2) - (d == 0);
      if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID || rooms[nx][ny].type == RT_NONE || dist[nx][ny] >= 0) continue;
      dist[nx][ny] = dist[cx][cy] + 1; qx[qt] = nx; qy[qt++] = ny;
    }
  }
  rooms[fx][fy].type = (floorN % 3 == 0) ? RT_BOSS : RT_STAIRS;
  if (rooms[fx][fy].type == RT_BOSS) rooms[fx][fy].tmpl = 0;
  int best = -1, bx = -1, by = -1;
  for (int i = 0; i < GRID; i++) for (int j = 0; j < GRID; j++) {
    if (rooms[i][j].type != RT_FIGHT) continue;
    int nb = 0;
    for (int d = 0; d < 4; d++) { int nx = i + (d == 1) - (d == 3), ny = j + (d == 2) - (d == 0);
      if (nx >= 0 && ny >= 0 && nx < GRID && ny < GRID && rooms[nx][ny].type) nb++; }
    int score = (nb == 1 ? 100 : 0) + dist[i][j] * 3 + rng.range(0, 4);
    if (score > best) { best = score; bx = i; by = j; }
  }
  if (bx >= 0) { rooms[bx][by].type = RT_TREASURE; rooms[bx][by].cleared = true; }
  rx = 2; ry = 2;
}

static Mob* spawnMob(uint8_t type, float x, float y) {
  for (auto& m : mobs) if (!m.alive) {
    m = {}; m.type = type; m.alive = true; m.x = x; m.y = y;
    int bonus = floorN / 3;
    m.hp = type == M_SLIME ? 2 + bonus : type == M_BAT ? 1 + bonus / 2 : type == M_ARCHER ? 2 + bonus : type == M_GUARD ? 4 + bonus
         : 22 + floorN * 4;
    m.maxHp = m.hp; m.t = rng.range(0, 40); m.cd = 40 + rng.range(0, 40);
    return &m;
  }
  return nullptr;
}

// moved: direction the player walked out of the last room (0 up 1 right 2 down 3 left, -1 = new floor)
static void enterRoom(int moved) {
  memset(mobs, 0, sizeof(mobs)); memset(shots, 0, sizeof(shots)); memset(drops, 0, sizeof(drops));
  Room& r = rooms[rx][ry];
  r.visited = true;
  switch (moved) {                                       // come in through the opposite door
    case 0:  px = 60; py = IY1 - PSZ - 1; break;
    case 2:  px = 60; py = IY0 + 1; break;
    case 1:  px = IX0 + 1; py = 32; break;
    case 3:  px = IX1 - PSZ - 1; py = 32; break;
    default: px = 60; py = 32; break;
  }
  chestHere = r.type == RT_TREASURE && !r.looted; chestX = 60; chestY = 32;
  stairsHere = (r.type == RT_STAIRS || r.type == RT_BOSS) && r.cleared;
  if (r.cleared) return;
  if (r.type == RT_BOSS) {
    Mob* b = spawnMob(M_BOSS, 57, 16);
    if (b) b->cd = 60;
    gSfx(220, 200); gHap(120, 8);
    return;
  }
  int n = min(2 + floorN / 2 + rng.range(0, 1), 6);
  for (int i = 0; i < n; i++) {
    uint8_t type = M_SLIME;
    int roll = rng.range(0, 99);
    if (floorN >= 4 && roll < 18) type = M_GUARD;
    else if (floorN >= 2 && roll < 45) type = M_ARCHER;
    else if (roll < 70) type = M_BAT;
    for (int tries = 0; tries < 30; tries++) {
      float x = IX0 + rng.range(0, TW - 1) * 8, y = IY0 + rng.range(0, TH - 1) * 8;
      if (fabsf(x - px) + fabsf(y - py) < 44) continue;
      if (blocked(x, y, 7, 7, type == M_BAT, false)) continue;
      spawnMob(type, x, y); break;
    }
  }
}

static void startRun() {
  sv.runs++; save();
  maxHp = 3 + sv.up[0]; hp = maxHp; dmg = 1 + sv.up[1]; speed = 1.15f + 0.18f * sv.up[2];
  floorN = 1; runCoins = 0; iframes = 0; swing = swingCd = 0; pkx = pky = 0; face = 1;
  rng = GRng(millis() ^ 0x5eedu);
  memset(sparks, 0, sizeof(sparks));
  buildFloor(); enterRoom(-1);
  state = 2; stateT = 0; kPause.open = false;
  gBase(NPX_SOLID, 0x402000, 3);
}

static void nextFloor() {
  floorN++; hp = min(hp + 1, maxHp);
  buildFloor(); enterRoom(-1);
  state = 2; stateT = 0;
  static const ToneStep t[] = { { 523, 80 }, { 659, 80 }, { 784, 80 }, { 1047, 160 } };
  if (S.gameSfx) buzzSeq(t, 4);
}

static void endRun() {
  sv.bank += runCoins;
  if (floorN > sv.best) sv.best = floorN;
  save();
  gameSubmitScore(floorN);
  state = 3; stateT = 0;
  gLose(); gHap(250, 9);
}

static void hurtPlayer(float fromX, float fromY) {
  if (iframes > 0 || state != 1) return;
  hp--; iframes = 40; shake = 6;
  float dx = px - fromX, dy = py - fromY, l = sqrtf(dx * dx + dy * dy) + 0.01f;
  pkx = dx / l * 3.0f; pky = dy / l * 3.0f;
  gSfx(180, 80); gHap(70, 8);
  if (hp <= 0) endRun();
}

static void dropLoot(float x, float y, bool boss) {
  int luck = sv.up[3];
  int coins = boss ? 6 : (rng.chance(55 + luck * 12) ? 1 : 0);
  uint8_t val = boss ? (uint8_t)(2 + floorN / 2) : (uint8_t)(1 + (floorN >= 4));
  if (boss) { x += 4; y += 4; }
  for (int i = 0; i < NDROP && coins > 0; i++) if (!drops[i].kind) {
    drops[i] = { D_COIN, x + rng.range(-8, 8), y + rng.range(-6, 6), 0, val }; coins--;
  }
  if (boss || rng.chance(7 + luck * 4))
    for (auto& d : drops) if (!d.kind) { d = { D_HEART, x, y, 0, 1 }; break; }
}

// ---- sword
static void swordBox(float& x, float& y, float& w, float& h) {
  const float r = 11;
  switch (face) {
    case 0: x = px - 3; y = py - r;  w = PSZ + 6; h = r; break;
    case 2: x = px - 3; y = py + PSZ; w = PSZ + 6; h = r; break;
    case 3: x = px - r; y = py - 3;  w = r; h = PSZ + 6; break;
    default: x = px + PSZ; y = py - 3; w = r; h = PSZ + 6; break;
  }
}

// ---- update
static void playerUpdate() {
  int dx = btnDown(BTN_RIGHT) - btnDown(BTN_LEFT), dy = btnDown(BTN_DOWN) - btnDown(BTN_UP);
  if (swing <= 0) {                                    // facing locks during a swing
    if (dx > 0) face = 1; else if (dx < 0) face = 3; else if (dy > 0) face = 2; else if (dy < 0) face = 0;
  }
  float sp = swing > 0 ? speed * 0.45f : speed;
  float mx = dx * sp, my = dy * sp;
  if (dx && dy) { mx *= 0.72f; my *= 0.72f; }
  mx += pkx; my += pky; pkx *= 0.7f; pky *= 0.7f;
  if (!blocked(px + mx, py, PSZ, PSZ, false, true)) px += mx;
  else if (!dy && fabsf(mx) > 0.1f) {                  // corner assist: slide around rock corners
    if (!blocked(px + mx, py - 1, PSZ, PSZ, false, true)) py -= 1;
    else if (!blocked(px + mx, py + 1, PSZ, PSZ, false, true)) py += 1;
  }
  if (!blocked(px, py + my, PSZ, PSZ, false, true)) py += my;
  else if (!dx && fabsf(my) > 0.1f) {
    if (!blocked(px - 1, py + my, PSZ, PSZ, false, true)) px -= 1;
    else if (!blocked(px + 1, py + my, PSZ, PSZ, false, true)) px += 1;
  }

  if (swingCd) swingCd--;
  if (btnPressed(BTN_SELECT) && swingCd == 0) { swing = 7; swingCd = 11; swingId++; gSfx(900, 18); }
  if (swing > 0) {
    swing--;
    float sx, sy, sw, sh; swordBox(sx, sy, sw, sh);
    for (auto& m : mobs) {
      if (!m.alive || m.hitSwing == (uint8_t)swingId) continue;
      int s = mobSize(m.type);
      if (!boxHit(sx, sy, sw, sh, m.x, m.y, s, s)) continue;
      m.hitSwing = (uint8_t)swingId;
      m.hp -= dmg;
      float ddx = m.x - px, ddy = m.y - py, l = sqrtf(ddx * ddx + ddy * ddy) + 0.01f;
      float kb = m.type == M_BOSS ? 0.8f : m.type == M_GUARD ? 1.6f : 3.2f;
      m.kx = ddx / l * kb; m.ky = ddy / l * kb;
      spark(m.x + s / 2, m.y + s / 2, 4);
      gHap(25, 6); gSfx(m.hp > 0 ? 420 : 260, 30);
      if (m.hp <= 0) {
        m.alive = false; spark(m.x + s / 2, m.y + s / 2, 8); shake = m.type == M_BOSS ? 14 : 3;
        dropLoot(m.x, m.y, m.type == M_BOSS);
        if (m.type == M_BOSS) { gWin(); gFlash(0xFFC000, 4, 120); }
      }
    }
    // swing cuts arrows out of the air
    for (auto& s : shots) if (s.on && !s.big && boxHit(sx, sy, sw, sh, s.x - 1, s.y - 1, 3, 3)) { s.on = false; spark(s.x, s.y, 2); }
  }
  if (iframes) iframes--;

  // pickups
  for (auto& d : drops) {
    if (!d.kind) continue;
    d.t++;
    float ddx = px + 3 - d.x, ddy = py + 3 - d.y, dist = fabsf(ddx) + fabsf(ddy);
    if (d.kind == D_COIN && dist < 22) { d.x += ddx * 0.18f; d.y += ddy * 0.18f; }   // magnet
    if (dist < 7) {
      if (d.kind == D_COIN) { runCoins += d.val; gSfx(2400, 12); }
      else if (hp < maxHp) { hp++; gSfx(1200, 60); gHap(30, 5); }
      else { runCoins += 3; gSfx(2000, 30); }
      d.kind = 0;
    }
  }
  if (chestHere && boxHit(px, py, PSZ, PSZ, chestX, chestY, 8, 8)) {
    chestHere = false; rooms[rx][ry].looted = true;
    int roll = rng.range(0, 2);
    if (roll == 0 && maxHp < 8) { maxHp++; hp = maxHp; say("Max heart up!"); }
    else if (roll == 1 && dmg < 5) { dmg++; say("Sword sharpened!"); }
    else { int c = 10 + floorN * 3; runCoins += c; char b[24]; snprintf(b, sizeof(b), "+%d coins", c); say(b); }
    static const ToneStep t[] = { { 784, 70 }, { 988, 70 }, { 1175, 70 }, { 1568, 180 } };
    if (S.gameSfx) buzzSeq(t, 4);
    gFlash(0xFFC000, 2, 120); gHap(60, 7);
  }
  if (stairsHere && stateT > 20 && boxHit(px + 2, py + 2, 3, 3, 58, 32, 8, 8)) { nextFloor(); return; }

  // leave through a door
  int dir = -1;
  if (py < IY0 - 5) dir = 0; else if (py + PSZ > IY1 + 5) dir = 2;
  else if (px < IX0 - 5) dir = 3; else if (px + PSZ > IX1 + 5) dir = 1;
  if (dir >= 0) {
    rx += (dir == 1) - (dir == 3); ry += (dir == 2) - (dir == 0);
    enterRoom(dir);
    pkx = pky = 0;
    gClick();
  }
}

static void fireShot(float x, float y, float vx, float vy, bool big) {
  for (auto& s : shots) if (!s.on) { s = { true, x, y, vx, vy, big }; return; }
}

static void moveMob(Mob& m, float vx, float vy) {
  int s = mobSize(m.type);
  bool fly = m.type == M_BAT;
  vx += m.kx; vy += m.ky; m.kx *= 0.75f; m.ky *= 0.75f;
  if (!blocked(m.x + vx, m.y, s, s, fly, false)) m.x += vx; else { m.vx = -m.vx; m.kx = 0; }
  if (!blocked(m.x, m.y + vy, s, s, fly, false)) m.y += vy; else { m.vy = -m.vy; m.ky = 0; }
}

static void mobsUpdate() {
  int alive = 0;
  for (auto& m : mobs) {
    if (!m.alive) continue;
    alive++;
    m.t++;
    int s = mobSize(m.type);
    float cx = m.x + s / 2.0f, cy = m.y + s / 2.0f;
    float dx = px + 3.5f - cx, dy = py + 3.5f - cy, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    switch (m.type) {
      case M_SLIME: {                                            // hops towards you
        int ph = m.t % 46;
        if (ph == 0) { m.vx = dx / d * 1.3f; m.vy = dy / d * 1.3f; }
        if (ph < 12) moveMob(m, m.vx, m.vy); else moveMob(m, 0, 0);
      } break;
      case M_BAT: {                                              // jittery swoop
        if (m.t % 20 == 0) { m.vx = dx / d * 0.9f + rng.range(-6, 6) / 10.0f; m.vy = dy / d * 0.9f + rng.range(-6, 6) / 10.0f; }
        moveMob(m, m.vx, m.vy + sinf(m.t * 0.3f) * 0.4f);
      } break;
      case M_ARCHER: {                                           // keeps distance, shoots along rows/columns
        float want = d < 34 ? -0.55f : d > 56 ? 0.45f : 0;
        moveMob(m, dx / d * want, dy / d * want);
        if (m.cd > 0) m.cd--;
        bool alignX = fabsf(dx) < 6, alignY = fabsf(dy) < 6;
        if (m.cd == 0 && (alignX || alignY || m.t % 150 == 0)) {
          float vx = alignX ? 0 : (dx > 0 ? 1.9f : -1.9f), vy = alignX ? (dy > 0 ? 1.9f : -1.9f) : 0;
          if (!alignX && !alignY) { vx = dx / d * 1.6f; vy = dy / d * 1.6f; }
          fireShot(cx, cy, vx, vy, false); m.cd = 70 - min(floorN * 3, 30); gSfx(1500, 15);
        }
      } break;
      case M_GUARD: {                                            // armoured: walks, then charges when lined up
        if (m.st == 0) {
          moveMob(m, dx / d * 0.4f, dy / d * 0.4f);
          if (m.cd > 0) m.cd--;
          if (m.cd == 0 && (fabsf(dx) < 5 || fabsf(dy) < 5)) { m.st = 1; m.t = 0; m.vx = fabsf(dx) < 5 ? 0 : (dx > 0 ? 1 : -1); m.vy = fabsf(dx) < 5 ? (dy > 0 ? 1 : -1) : 0; }
        } else if (m.st == 1) {                                  // wind-up shake
          if (m.t > 14) { m.st = 2; m.t = 0; gSfx(300, 40); }
        } else if (m.st == 2) {                                  // charge until a wall
          float ox = m.x, oy = m.y;
          moveMob(m, m.vx * 2.8f, m.vy * 2.8f);
          if ((fabsf(m.x - ox) + fabsf(m.y - oy) < 0.5f) || m.t > 40) { m.st = 3; m.t = 0; shake = 3; }
        } else if (m.t > 40) { m.st = 0; m.cd = 50; }            // dazed (st 3)
      } break;
      case M_BOSS: {
        bool angry = m.hp * 2 < m.maxHp;
        if (m.st == 0) {                                         // stalk
          moveMob(m, dx / d * (angry ? 0.6f : 0.4f), dy / d * (angry ? 0.6f : 0.4f));
          if (--m.cd <= 0) { m.st = 1 + rng.range(0, angry ? 2 : 1); m.t = 0; }
        } else if (m.st == 1) {                                  // ring of seeds
          if (m.t == 12 || (angry && m.t == 30)) {
            int n = angry ? 10 : 8; float off = m.t == 30 ? 0.3f : 0;
            for (int i = 0; i < n; i++) { float a = i * 6.2832f / n + off; fireShot(cx, cy, cosf(a) * 1.3f, sinf(a) * 1.3f, true); }
            gSfx(200, 80); shake = 4;
          }
          if (m.t > 40) { m.st = 0; m.cd = 70; }
        } else if (m.st == 2) {                                  // charge
          if (m.t == 1) { m.vx = dx / d; m.vy = dy / d; }
          if (m.t > 16) {
            float ox = m.x, oy = m.y;
            moveMob(m, m.vx * 2.6f, m.vy * 2.6f);
            if ((fabsf(m.x - ox) + fabsf(m.y - oy) < 0.5f) || m.t > 60) { m.st = 0; m.cd = 60; shake = 6; gHap(80, 8); }
          }
        } else {                                                 // summon (angry only)
          if (m.t == 10) for (int k = 0; k < 2; k++) {
            float sx = m.x + (k ? 18 : -10), sy = m.y + 4;
            if (!blocked(sx, sy, 7, 7, false, false)) spawnMob(rng.chance(50) ? M_SLIME : M_BAT, sx, sy);
          }
          if (m.t > 30) { m.st = 0; m.cd = 90; }
        }
      } break;
    }
    if (m.alive && boxHit(px + 1, py + 1, PSZ - 2, PSZ - 2, m.x + 1, m.y + 1, s - 2, s - 2)) hurtPlayer(cx, cy);
  }
  for (auto& s : shots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if (s.x < IX0 || s.x > IX1 || s.y < IY0 || s.y > IY1) { s.on = false; continue; }
    int tx = (int)((s.x - IX0) / 8), ty = (int)((s.y - IY0) / 8);
    if (tileAt(tx, ty) == '#') { s.on = false; spark(s.x, s.y, 2); continue; }
    if (boxHit(px + 1, py + 1, PSZ - 2, PSZ - 2, s.x - 1, s.y - 1, 3, 3)) { s.on = false; hurtPlayer(s.x - s.vx * 4, s.y - s.vy * 4); }
  }
  Room& r = rooms[rx][ry];
  if (!r.cleared && alive == 0) {
    r.cleared = true;
    if (r.type == RT_STAIRS || r.type == RT_BOSS) { stairsHere = true; stateT = 0; say("Stairs opened"); }
    static const ToneStep t[] = { { 659, 60 }, { 988, 120 } };
    if (S.gameSfx) buzzSeq(t, 2);
    gHap(40, 5);
  }
}

// ---- drawing
static void drawMsg(int y) {
  if (msgT <= 0) return;
  msgT--;
  u8g2.setFont(F_SMALL);
  int w = u8g2.getStrWidth(msg) + 8;
  u8g2.setDrawColor(0); u8g2.drawBox(64 - w / 2, y - 9, w, 12); u8g2.setDrawColor(1);
  u8g2.drawRFrame(64 - w / 2, y - 9, w, 12, 2);
  u8g2.drawStr(64 - w / 2 + 4, y, msg);
}
static void drawHud() {
  u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 128, 8); u8g2.setDrawColor(1);
  for (int i = 0; i < maxHp; i++) {                            // 5x4 hearts
    int x = 1 + i * 7, y = 1;
    bool full = i < hp;
    u8g2.drawPixel(x, y); u8g2.drawPixel(x + 1, y); u8g2.drawPixel(x + 3, y); u8g2.drawPixel(x + 4, y);
    if (full) { u8g2.drawHLine(x, y + 1, 5); u8g2.drawHLine(x + 1, y + 2, 3); }
    else { u8g2.drawPixel(x, y + 1); u8g2.drawPixel(x + 4, y + 1); u8g2.drawPixel(x + 1, y + 2); u8g2.drawPixel(x + 3, y + 2); }
    u8g2.drawPixel(x + 2, y + 3); if (full) u8g2.drawPixel(x + 2, y + 2);
  }
  u8g2.setFont(F_TINY);
  char b[16];
  snprintf(b, sizeof(b), "o%d", runCoins); u8g2.drawStr(64, 6, b);
  snprintf(b, sizeof(b), "F%d", floorN); u8g2.drawStr(127 - u8g2.getStrWidth(b), 6, b);
  for (const auto& m : mobs) if (m.alive && m.type == M_BOSS) {   // boss bar replaces coins
    u8g2.setDrawColor(0); u8g2.drawBox(56, 0, 56, 8); u8g2.setDrawColor(1);
    u8g2.drawFrame(60, 1, 48, 5); u8g2.drawBox(61, 2, (46 * max(0, m.hp)) / m.maxHp, 3);
  }
}

static void drawRoom(int ox, int oy) {
  // walls with door gaps
  u8g2.drawBox(ox, oy + 8, 128, 4); u8g2.drawBox(ox, oy + 60, 128, 4);
  u8g2.drawBox(ox, oy + 8, 4, 56);  u8g2.drawBox(ox + 124, oy + 8, 4, 56);
  u8g2.setDrawColor(0);
  for (int x = 6; x < 128; x += 8) { u8g2.drawPixel(ox + x, oy + 9); u8g2.drawPixel(ox + x + 4, oy + 62); }
  bool open = doorsOpen();
  if (hasDoor(0)) u8g2.drawBox(ox + 56, oy + 8, 16, 4);
  if (hasDoor(2)) u8g2.drawBox(ox + 56, oy + 60, 16, 4);
  if (hasDoor(3)) u8g2.drawBox(ox, oy + 28, 4, 16);
  if (hasDoor(1)) u8g2.drawBox(ox + 124, oy + 28, 4, 16);
  u8g2.setDrawColor(1);
  if (!open) {                                                  // bars
    if (hasDoor(0)) for (int x = 57; x < 72; x += 3) u8g2.drawVLine(ox + x, oy + 8, 4);
    if (hasDoor(2)) for (int x = 57; x < 72; x += 3) u8g2.drawVLine(ox + x, oy + 60, 4);
    if (hasDoor(3)) for (int y = 29; y < 44; y += 3) u8g2.drawHLine(ox, oy + y, 4);
    if (hasDoor(1)) for (int y = 29; y < 44; y += 3) u8g2.drawHLine(ox + 124, oy + y, 4);
  }
  for (int ty = 0; ty < TH; ty++) for (int tx = 0; tx < TW; tx++) {
    char t = tileAt(tx, ty);
    int sx = ox + IX0 + tx * 8, sy = oy + IY0 + ty * 8;
    if (t == '#') spr8(sx, sy, SP_ROCK, 8);
    else if (t == 'o') { u8g2.drawFrame(sx, sy, 8, 8); for (int y = 2; y < 7; y += 2) for (int x = 2 + (y & 2) / 2; x < 7; x += 2) u8g2.drawPixel(sx + x, sy + y); }
  }
  if (stairsHere) { u8g2.drawFrame(ox + 58, oy + 32, 8, 8); u8g2.drawFrame(ox + 60, oy + 34, 4, 4); u8g2.drawPixel(ox + 61, oy + 35); }
  if (chestHere) spr8(ox + (int)chestX, oy + (int)chestY + (((millis() / 400) & 1) ? 0 : -1), SP_CHEST, 8);
}

static void drawScene() {
  int ox = 0, oy = 0;
  if (shake > 0) { ox = rng.range(-1, 1) * (shake > 6 ? 2 : 1); oy = rng.range(-1, 1); shake--; }
  drawRoom(ox, oy);
  for (const auto& d : drops) {
    if (!d.kind) continue;
    int x = ox + (int)d.x, y = oy + (int)d.y;
    if (d.kind == D_COIN) { u8g2.drawCircle(x + 2, y + 2, 2); if ((d.t / 8) & 1) u8g2.drawPixel(x + 2, y + 2); }
    else spr8(x - 1, y - 1, SP_HEART, 8);
  }
  for (const auto& m : mobs) {
    if (!m.alive) continue;
    int x = ox + (int)m.x, y = oy + (int)m.y;
    bool flip = px < m.x;
    switch (m.type) {
      case M_SLIME:  spr8o(x, y - 1, (m.t % 46) < 12 ? SP_SLIME2 : SP_SLIME, 8, flip); break;
      case M_BAT:    spr8o(x, y, ((m.t / 5) & 1) ? SP_BAT2 : SP_BAT, 8, flip); break;
      case M_ARCHER: spr8o(x, y - 1, SP_ARCH, 8, flip); break;
      case M_GUARD:  spr8o(x + (m.st == 1 ? ((m.t & 2) ? 1 : -1) : 0), y - 1, SP_GUARD, 8, flip);
                     if (m.st == 3) { u8g2.drawPixel(x + 1, y - 3); u8g2.drawPixel(x + 4, y - 4); u8g2.drawPixel(x + 7, y - 3); } break;
      case M_BOSS: {
        bool tell = (m.st == 1 && m.t < 12) || (m.st == 2 && m.t < 16);
        int jx = tell ? ((m.t & 2) ? 1 : -1) : 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) spr16(x - 1 + jx + dx, y - 1 + dy, SP_BOSS, 16, flip, 0);
        spr16(x - 1 + jx, y - 1, SP_BOSS, 16, flip, 1);
      } break;
    }
  }
  for (const auto& s : shots) {
    if (!s.on) continue;
    int x = ox + (int)s.x, y = oy + (int)s.y;
    if (s.big) { u8g2.setDrawColor(0); u8g2.drawDisc(x, y, 2); u8g2.setDrawColor(1); u8g2.drawCircle(x, y, 2); }
    else { u8g2.drawLine(x, y, x - (int)(s.vx * 2), y - (int)(s.vy * 2)); }
  }
  // player (blinks while invulnerable)
  if (state != 1 || !iframes || (iframes / 3) % 2) {
    const uint8_t* sp = face == 0 ? SK_UP : face == 2 ? SK_DOWN : SK_SIDE;
    int bob = (btnDown(BTN_LEFT) || btnDown(BTN_RIGHT) || btnDown(BTN_UP) || btnDown(BTN_DOWN)) && ((millis() / 140) & 1);
    spr8o(ox + (int)px, oy + (int)py - 1 - bob, sp, 8, face == 3);
  }
  if (swing > 0) {
    int cx = ox + (int)px + 3, cy = oy + (int)py + 3;
    int f = 7 - swing;                                          // 0..6 arc from -60 to +60 degrees
    float base = face == 0 ? -1.5708f : face == 1 ? 0 : face == 2 ? 1.5708f : 3.1416f;
    float a = base + (f - 3) * 0.38f * (face == 3 || face == 0 ? -1 : 1);
    int x1 = cx + (int)(cosf(a) * 4), y1 = cy + (int)(sinf(a) * 4);
    int x2 = cx + (int)(cosf(a) * 12), y2 = cy + (int)(sinf(a) * 12);
    u8g2.drawLine(x1, y1, x2, y2);
    u8g2.drawLine(x1 + (int)(sinf(a) * 2), y1 - (int)(cosf(a) * 2), x1 - (int)(sinf(a) * 2), y1 + (int)(cosf(a) * 2));
    for (int k = -2; k <= 0; k++) { float b2 = a - k * 0.3f * (face == 3 || face == 0 ? -1 : 1);
      if (k) u8g2.drawPixel(cx + (int)(cosf(b2) * 13), cy + (int)(sinf(b2) * 13)); }
  }
  for (auto& sp : sparks) if (sp.t > 0) { sp.x += sp.vx; sp.y += sp.vy; sp.t--; u8g2.drawPixel(ox + (int)sp.x, oy + (int)sp.y); }
  drawHud();
  drawMsg(50);
}

static void drawMap() {
  u8g2.clearBuffer();
  u8g2.setFont(F_UI); u8g2.drawStr(2, 10, "Map");
  u8g2.setFont(F_TINY);
  char b[24]; snprintf(b, sizeof(b), "floor %d", floorN); u8g2.drawStr(126 - u8g2.getStrWidth(b), 9, b);
  const int cw = 14, ch = 9, x0 = 64 - (GRID * cw) / 2, y0 = 14;
  for (int i = 0; i < GRID; i++) for (int j = 0; j < GRID; j++) {
    const Room& r = rooms[i][j];
    if (!r.type) continue;
    bool near = false;
    for (int d = 0; d < 4; d++) { int nx = i + (d == 1) - (d == 3), ny = j + (d == 2) - (d == 0);
      if (nx >= 0 && ny >= 0 && nx < GRID && ny < GRID && rooms[nx][ny].visited) near = true; }
    if (!r.visited && !near) continue;
    int x = x0 + i * cw, y = y0 + j * ch;
    if (i == rx && j == ry) u8g2.drawBox(x, y, cw - 2, ch - 2);
    else if (r.visited) u8g2.drawFrame(x, y, cw - 2, ch - 2);
    else for (int k = 0; k < cw - 2; k += 2) { u8g2.drawPixel(x + k, y); u8g2.drawPixel(x + k, y + ch - 3); }
    if (r.visited && !(i == rx && j == ry)) {
      if (r.type == RT_STAIRS || r.type == RT_BOSS) u8g2.drawStr(x + 4, y + 6, r.type == RT_BOSS ? "B" : ">");
      if (r.type == RT_TREASURE && !r.looted) u8g2.drawStr(x + 4, y + 6, "$");
    }
  }
  u8g2.drawStr(2, 63, "any key: back");
  gfxFlush();
}

// ---- camp (title + upgrades)
static void campLoop() {
  if (btnLong(BTN_SELECT)) { gameReturnToHub(); return; }
  if (btnRepeat(BTN_DOWN)) { campSel = (campSel + 1) % 2; gClick(); }
  if (btnRepeat(BTN_UP))   { campSel = (campSel + 1) % 2; gClick(); }
  if (btnTap(BTN_SELECT)) { if (campSel == 0) { startRun(); return; } state = 5; upSel = 0; gClick(); return; }
  u8g2.clearBuffer();
  u8g2.setFont(F_MEDB); u8g2.drawStr(64 - u8g2.getStrWidth("SPUD KNIGHT") / 2, 13, "SPUD KNIGHT");
  spr8o(18, 20, SK_SIDE, 8); spr8o(102, 20, SP_SLIME, 8, true);
  u8g2.setFont(F_SMALL);
  const char* items[2] = { "Start run", "Upgrades" };
  for (int i = 0; i < 2; i++) {
    int y = 30 + i * 11, w = u8g2.getStrWidth(items[i]);
    if (i == campSel) { u8g2.drawRBox(64 - w / 2 - 4, y - 8, w + 8, 10, 2); u8g2.setDrawColor(0); }
    u8g2.drawStr(64 - w / 2, y, items[i]); u8g2.setDrawColor(1);
  }
  u8g2.setFont(F_TINY);
  char b[40]; snprintf(b, sizeof(b), "bank o%lu   best floor %u", (unsigned long)sv.bank, sv.best);
  u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 55, b);
  u8g2.drawStr(64 - u8g2.getStrWidth("SEL choose   hold SEL exit") / 2, 63, "SEL choose   hold SEL exit");
  gfxFlush();
}

static void upgradesLoop() {
  if (btnTap(BTN_LEFT) || btnLong(BTN_SELECT)) { state = 0; gClick(); return; }
  if (btnRepeat(BTN_DOWN)) { upSel = (upSel + 1) % 4; gClick(); }
  if (btnRepeat(BTN_UP))   { upSel = (upSel + 3) % 4; gClick(); }
  int lvl = sv.up[upSel];
  uint16_t cost = lvl < UP_MAX[upSel] ? UP_COST[upSel][lvl] : 0;
  if (btnTap(BTN_SELECT)) {
    if (!cost) gSfx(200, 60);
    else if (sv.bank < cost) { gSfx(200, 60); say("Not enough coins"); }
    else { sv.bank -= cost; sv.up[upSel]++; save(); gWin(); }
  }
  u8g2.clearBuffer();
  u8g2.setFont(F_UI); u8g2.drawStr(2, 10, "Upgrades");
  u8g2.setFont(F_TINY);
  char b[32]; snprintf(b, sizeof(b), "o%lu", (unsigned long)sv.bank); u8g2.drawStr(126 - u8g2.getStrWidth(b), 9, b);
  u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(F_SMALL);
  for (int i = 0; i < 4; i++) {
    int y = 22 + i * 9;
    if (i == upSel) { u8g2.drawBox(0, y - 8, 128, 9); u8g2.setDrawColor(0); }
    u8g2.drawStr(3, y, UP_NAME[i]);
    for (int k = 0; k < UP_MAX[i]; k++) { if (k < sv.up[i]) u8g2.drawBox(40 + k * 6, y - 5, 4, 4); else u8g2.drawFrame(40 + k * 6, y - 5, 4, 4); }
    uint16_t c = sv.up[i] < UP_MAX[i] ? UP_COST[i][sv.up[i]] : 0;
    if (c) snprintf(b, sizeof(b), "o%u", c); else snprintf(b, sizeof(b), "max");
    u8g2.drawStr(125 - u8g2.getStrWidth(b), y, b);
    u8g2.setDrawColor(1);
  }
  u8g2.setFont(F_TINY);
  u8g2.drawStr(2, 63, UP_DESC[upSel]);
  u8g2.drawStr(126 - u8g2.getStrWidth("SEL buy  < back"), 63, "SEL buy  < back");
  drawMsg(40);
  gfxFlush();
}

// ---- entry points
void knightEnter() { loadSave(); state = 0; campSel = 0; }

void knightLoop() {
  if (state == 0) { campLoop(); return; }
  if (state == 5) { upgradesLoop(); return; }
  if (state == 4) {
    drawMap();
    if (btnTap(BTN_SELECT) || btnTap(BTN_LEFT) || btnTap(BTN_RIGHT) || btnTap(BTN_UP) || btnTap(BTN_DOWN)) state = 1;
    return;
  }
  static const char* const EXTRA[] = { "Map" };
  if (state == 1 && btnLong(BTN_SELECT) && !kPause.open) { kPause.open = true; kPause.sel = 0; swing = 0; gClick(); }
  int pr = kPause.update(EXTRA, 1);
  if (pr == 2) { endRun(); state = 0; return; }                  // quitting banks your coins
  if (pr == 3) { state = 4; return; }

  if (!kPause.open) {
    if (state == 1) { stateT++; playerUpdate(); if (state == 1) mobsUpdate(); }
    else if (state == 2) { if (++stateT > 40) state = 1; }
    else if (state == 3) { stateT++; if (stateT > 20 && btnTap(BTN_SELECT)) { state = 0; return; } }
  }

  u8g2.clearBuffer();
  drawScene();
  if (state == 2) {
    u8g2.setDrawColor(0); u8g2.drawBox(30, 22, 68, 22); u8g2.setDrawColor(1); u8g2.drawRFrame(30, 22, 68, 22, 3);
    u8g2.setFont(F_MEDB); char b[16]; snprintf(b, sizeof(b), "Floor %d", floorN);
    u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 35, b);
    u8g2.setFont(F_TINY);
    const char* sub = floorN % 3 == 0 ? "a boss waits below" : "find the stairs";
    u8g2.drawStr(64 - u8g2.getStrWidth(sub) / 2, 42, sub);
  }
  if (state == 3) {
    u8g2.setDrawColor(0); u8g2.drawBox(14, 12, 100, 46); u8g2.setDrawColor(1); u8g2.drawRFrame(14, 12, 100, 46, 3);
    u8g2.setFont(F_MEDB); u8g2.drawStr(64 - u8g2.getStrWidth("FALLEN") / 2, 26, "FALLEN");
    u8g2.setFont(F_SMALL);
    char b[32];
    snprintf(b, sizeof(b), "floor %d%s", floorN, floorN >= sv.best ? "  best!" : ""); u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 36, b);
    snprintf(b, sizeof(b), "+%d coins banked", runCoins); u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 45, b);
    u8g2.setFont(F_TINY); u8g2.drawStr(64 - u8g2.getStrWidth("SEL to camp") / 2, 54, "SEL to camp");
  }
  kPause.draw("Paused", EXTRA, 1);
  gfxFlush();
}
