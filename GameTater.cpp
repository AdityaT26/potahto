// "Tater Run": a proper little platformer
//
// 4 worlds x 6 levels (Fields, Caves, Sky, Factory). Each level is built from
// hand-designed 16x8-tile chunks picked by a fixed seed, so a level is always
// the same level: learn it, beat your time, find all 3 golden fries.
//
// Controls  LEFT/RIGHT run    UP jump (hold = higher)    DOWN drop through
//           thin platforms    SELECT pause menu
// Feel      coyote time, jump buffering, variable jump height, stomp bounce.
//           Works on a single 5-way switch too: rolling from RIGHT to UP keeps
//           your speed, and rolling back to a direction mid-air keeps the jump
//           high (only letting go of everything cuts it short).
// Saves     unlocked levels, fries per level and best times ("gTater").
#include "Multitool.h"
#include "GameGfx.h"

#define CH_W    16
#define CH_H    8
#define MAX_CH  18
#define LVL_W   (MAX_CH * CH_W)
#define NWORLD  4
#define NLEV    6

// ---- chunk library
//  #  ground     =  thin platform (jump up through, DOWN to drop)
//  ^  spike      v  ceiling spike   c coin   f bonus spot (fry or coin)
//  e  walker     h  hopper          m moving platform (3 wide, travels right 7)
//  C  checkpoint G  goal
enum : uint8_t { TH_FIELDS = 1, TH_CAVES = 2, TH_SKY = 4, TH_FACTORY = 8, TH_ALL = 15 };
struct Chunk { uint8_t diff, themes; const char* r[CH_H]; };
static const Chunk START = { 0, TH_ALL, { "................", "................", "................", "................",
                                          "................", "................", "################", "################" } };
static const Chunk CHECK = { 0, TH_ALL, { "................", "................", "................", "................",
                                          "................", "........C.......", "################", "################" } };
static const Chunk GOAL  = { 0, TH_ALL, { "................", "................", "................", "................",
                                          "................", "..........G.....", "################", "################" } };
static const Chunk CHUNKS[] = {
  { 1, TH_ALL,     { "................", "................", "................", "................",
                     "....c..c..c.....", "................", "################", "################" } },
  { 1, TH_ALL,     { "................", "................", "................", ".......f........",
                     "................", "................", "######..########", "######..########" } },
  { 1, TH_ALL & ~TH_CAVES, { "................", "................", "...........f....", "..........###...",
                     "......c...###...", "....####..###...", "################", "################" } },
  { 1, TH_ALL,     { "................", "................", "................", "......ccc.......",
                     "................", "..........e.....", "################", "################" } },
  { 1, TH_ALL,     { "................", "................", ".......f........", "......###.......",
                     "....#######.....", "..###########...", "################", "################" } },
  { 2, TH_ALL,     { "................", "................", ".......f........", "......====......",
                     "................", "..===......===..", "##............##", "##............##" } },
  { 2, TH_ALL,     { "................", "................", "................", ".....ccc........",
                     "................", "....^^^....^^...", "################", "################" } },
  { 2, TH_ALL,     { "................", "................", "..........f.....", ".....#####......",
                     "................", "...........h....", "################", "################" } },
  { 2, TH_ALL,     { "................", "................", "................", "....c.....c.....",
                     "................", "................", "##...####...####", "##...####...####" } },
  { 2, TH_ALL,     { "................", "................", "................", ".......cc.......",
                     "......####......", "...e........e...", "################", "################" } },
  { 2, TH_CAVES,   { "################", "################", "..vvvv....vvvv..", "................",
                     "................", "......cccc......", "################", "################" } },
  { 2, TH_SKY | TH_FIELDS, { "................", "................", "................", "......f.........",
                     ".....###........", "..........###...", "###...........##", "###...........##" } },
  { 3, TH_ALL,     { "................", "................", "...........f....", "...........#....",
                     ".......#...#....", "...#...#...#....", "##.#...#...#..##", "##.#...#...#..##" } },
  { 3, TH_ALL,     { "................", "................", "................", "................",
                     "......=====.....", "................", "###..^^^^^^..###", "################" } },
  { 3, TH_ALL,     { "................", "................", "................", "........c.......",
                     "................", "......===.......", "####........####", "####........####" } },
  { 3, TH_SKY | TH_FIELDS | TH_FACTORY, { "................", "................", "................", "....c..c..c.....",
                     "................", "...m............", "###..........###", "###..........###" } },
  { 3, TH_FACTORY | TH_CAVES, { "................", "................", "................", "....c..c..c.....",
                     "................", "...m............", "###^^^^^^^^^^###", "################" } },
};
static const int NCHUNKS = sizeof(CHUNKS) / sizeof(CHUNKS[0]);
static const char* WORLD_NAME[NWORLD] = { "Fields", "Caves", "Sky", "Factory" };
static const uint8_t WORLD_THEME[NWORLD] = { TH_FIELDS, TH_CAVES, TH_SKY, TH_FACTORY };

// ---- sprites
static const uint8_t SP_IDLE[8] = { R8("..XXXX.."), R8(".XXXXXX."), R8("XX.XX.XX"), R8("XXXXXXXX"),
                                    R8("XXX..XXX"), R8(".XXXXXX."), R8("..X..X.."), R8(".XX..XX.") };
static const uint8_t SP_RUN[8]  = { R8("..XXXX.."), R8(".XXXXXX."), R8("XX.XX.XX"), R8("XXXXXXXX"),
                                    R8("XXX..XXX"), R8(".XXXXXX."), R8(".X....X."), R8("X......X") };
static const uint8_t SP_JUMP[8] = { R8("X.XXXX.X"), R8(".XXXXXX."), R8("XX.XX.XX"), R8("XXXXXXXX"),
                                    R8("XXXXXXXX"), R8(".XXXXXX."), R8("..XXXX.."), R8("........") };
static const uint8_t SP_WALK[8] = { R8("........"), R8("..XXXX.."), R8(".X.XX.X."), R8("XXXXXXXX"),
                                    R8("X.XXXX.X"), R8("XXXXXXXX"), R8(".X.X.X.X"), R8("X.X.X.X.") };
static const uint8_t SP_HOP[8]  = { R8("..X..X.."), R8("..XXXX.."), R8(".XXXXXX."), R8("XX.XX.XX"),
                                    R8("XXXXXXXX"), R8(".XXXXXX."), R8(".X....X."), R8("XX....XX") };
static const uint8_t SP_FRY[8]  = { R8("...X...."), R8("..XXX..."), R8("XXXXXXX."), R8(".XXXXX.."),
                                    R8("..XXX..."), R8(".XX.XX.."), R8(".X...X.."), R8("........") };

// ---- level state
struct Ent { uint8_t type; bool alive; float x, y, vx, vy, x0, x1; uint16_t t; };
enum : uint8_t { E_WALK = 1, E_HOP, E_PLAT };
static char  L[CH_H][LVL_W];
static int   lvlW = 0;
static Ent   ents[28]; static int nEnt = 0;
struct FrySpot { int16_t x, y; bool got; };
static FrySpot fries[3];
static int   world = 0, lev = 0;

// player
static float px, py, pvx, pvy, prevBottom;
static bool  onGround, facingLeft;
static int   coyote, jumpBuf, dropT, ridePlat;
static float spawnX, spawnY, cam;
static int   coins, deaths;
static uint32_t elapsed, lastMs;
static uint8_t  state;                 // 0 select, 1 play, 2 dead, 3 clear
static int      stateT;
static bool     newBest;
static PauseMenu tPause;

#define PW 6
#define PH 7

// ---- save
struct TaterSave { uint8_t unlocked; uint8_t fry[NWORLD * NLEV]; uint16_t best[NWORLD * NLEV]; uint32_t coins; };
static TaterSave sv;
static void loadSave() { if (!gameLoad("gTater", sv)) { memset(&sv, 0, sizeof(sv)); sv.unlocked = 1; } }
static int  totalFries() { int n = 0; for (int i = 0; i < NWORLD * NLEV; i++) n += __builtin_popcount(sv.fry[i]); return n; }

// ---- level build
static void putChunk(const Chunk& c, int col, int16_t* fx, int16_t* fy, int& nf, GRng& rng) {
  for (int y = 0; y < CH_H; y++) for (int x = 0; x < CH_W; x++) {
    char ch = c.r[y][x], out = ' ';
    int ax = col + x;
    switch (ch) {
      case '#': case '=': case '^': case 'v': case 'c': case 'C': case 'G': out = ch; break;
      case 'f': if (nf < 32) { fx[nf] = ax; fy[nf] = y; nf++; } break;
      case 'e': case 'h': if (nEnt < 28) { Ent& e = ents[nEnt++]; e = {}; e.type = ch == 'e' ? E_WALK : E_HOP; e.alive = true;
                  e.x = ax * 8; e.y = y * 8 + 2; e.vx = rng.chance(50) ? -0.45f : 0.45f; e.t = rng.range(0, 60); } break;
      case 'm': if (nEnt < 28) { Ent& e = ents[nEnt++]; e = {}; e.type = E_PLAT; e.alive = true;
                  e.x = e.x0 = ax * 8; e.x1 = (ax + 7) * 8; e.y = y * 8; e.vx = 0.6f; } break;
    }
    L[y][ax] = out;
  }
}
static void buildLevel() {
  GRng rng(7919u * (world + 1) + 104729u * (lev + 1));
  memset(L, ' ', sizeof(L)); nEnt = 0;
  int n = min(MAX_CH - 2, 6 + lev + world);
  int maxDiff = min(3, 1 + world + lev / 3);
  int16_t fx[32], fy[32]; int nf = 0;
  putChunk(START, 0, fx, fy, nf, rng);
  int last = -1;
  for (int i = 1; i <= n; i++) {
    if (i == n / 2 + 1) { putChunk(CHECK, i * CH_W, fx, fy, nf, rng); continue; }
    int pick = 0;
    for (int tries = 0; tries < 60; tries++) {
      pick = rng.range(0, NCHUNKS - 1);
      const Chunk& c = CHUNKS[pick];
      if (c.diff > maxDiff || !(c.themes & WORLD_THEME[world]) || pick == last) continue;
      if (c.diff < maxDiff - 1 && rng.chance(60)) continue;       // lean towards harder ones later on
      break;
    }
    last = pick;
    putChunk(CHUNKS[pick], i * CH_W, fx, fy, nf, rng);
  }
  putChunk(GOAL, (n + 1) * CH_W, fx, fy, nf, rng);
  lvlW = (n + 2) * CH_W;
  // 3 fries spread over the level; leftover bonus spots become coins
  for (int k = 0; k < 3; k++) {
    if (nf >= 3) { int idx = (nf * (2 * k + 1)) / 6; fries[k] = { fx[idx], fy[idx], false }; fx[idx] = -1; }
    else {                                                        // not enough spots: steal coins
      int found = -1;
      for (int x = lvlW * (k + 1) / 4; x < lvlW && found < 0; x++) for (int y = 0; y < CH_H; y++) if (L[y][x] == 'c') { found = x; L[y][x] = ' '; fries[k] = { (int16_t)x, (int16_t)y, false }; break; }
      if (found < 0) fries[k] = { (int16_t)(lvlW / 2), 3, false };
    }
  }
  for (int i = 0; i < nf; i++) if (fx[i] >= 0) L[fy[i]][fx[i]] = 'c';
  uint8_t had = sv.fry[world * NLEV + lev];
  for (int k = 0; k < 3; k++) fries[k].got = false; (void)had;
}

static void respawn() {
  px = spawnX; py = spawnY; pvx = pvy = 0; onGround = false; coyote = jumpBuf = dropT = 0; ridePlat = -1;
  cam = max(0.0f, px - 48);
}
static void startLevel() {
  buildLevel();
  spawnX = 16; spawnY = 40; coins = 0; deaths = 0; elapsed = 0; lastMs = millis();
  respawn();
  state = 1; stateT = 0; tPause.open = false;
  static const uint32_t COL[NWORLD] = { 0x00A020, 0x6000A0, 0x0060C0, 0xA06000 };
  gBase(NPX_SOLID, COL[world], 3);
}

// ---- collision
static inline char tileAt(int tx, int ty) { if (tx < 0 || tx >= lvlW) return '#'; if (ty < 0 || ty >= CH_H) return ' '; return L[ty][tx]; }
static inline bool solid(int tx, int ty) { return tileAt(tx, ty) == '#'; }
static bool solidBox(float x, float y, float w, float h) {
  for (int ty = (int)floorf(y / 8); ty <= (int)floorf((y + h - 0.01f) / 8); ty++)
    for (int tx = (int)floorf(x / 8); tx <= (int)floorf((x + w - 0.01f) / 8); tx++) if (solid(tx, ty)) return true;
  return false;
}

static void die() {
  if (state != 1) return;
  state = 2; stateT = 0; deaths++;
  gLose(); gHap(80, 7);
}

static void playerUpdate() {
  static int letGo = 0;                                           // frames since a direction was released
  bool left = btnDown(BTN_LEFT), right = btnDown(BTN_RIGHT);
  bool jumpHeld = btnDown(BTN_UP) || left || right;               // any held key keeps the jump high
  if (btnPressed(BTN_UP)) jumpBuf = 5;
  if (btnPressed(BTN_DOWN) && onGround) dropT = 10;
  if (left || right) letGo = 0; else if (letGo < 99) letGo++;

  float acc = onGround ? 0.42f : 0.26f, maxV = 2.0f;
  if (left && !right)      { pvx -= acc; facingLeft = true; }
  else if (right && !left) { pvx += acc; facingLeft = false; }
  else if (onGround && letGo > 5) { pvx *= 0.55f; if (fabsf(pvx) < 0.05f) pvx = 0; }   // short grace: RIGHT->UP roll keeps speed
  else if (!onGround)      { pvx *= 0.985f; }                        // air keeps momentum
  pvx = clampv(pvx, -maxV, maxV);

  if (onGround) coyote = 6; else if (coyote) coyote--;
  if (jumpBuf) jumpBuf--;
  if (dropT) dropT--;
  if (jumpBuf && coyote) { pvy = -4.0f; coyote = 0; jumpBuf = 0; onGround = false; ridePlat = -1; gSfx(1400, 14); }
  if (!jumpHeld && pvy < -1.5f) pvy = -1.5f;                    // short hop when released early
  pvy = min(pvy + 0.32f, 4.2f);

  // ride moving platform
  if (ridePlat >= 0) px += ents[ridePlat].vx;

  // X
  float nx = px + pvx;
  if (solidBox(nx, py, PW, PH)) {
    if (pvx > 0) nx = floorf((nx + PW) / 8) * 8 - PW; else nx = floorf(nx / 8) * 8 + 8;
    pvx = 0;
  }
  px = clampv(nx, 0.0f, (float)(lvlW * 8 - PW));
  // Y
  prevBottom = py + PH;
  float ny = py + pvy;
  onGround = false;
  if (pvy > 0) {
    float bottom = ny + PH;
    int ty = (int)floorf(bottom / 8);
    bool land = false;
    for (int tx = (int)floorf(px / 8); tx <= (int)floorf((px + PW - 0.01f) / 8); tx++) {
      char t = tileAt(tx, ty);
      if (t == '#' || (t == '=' && !dropT && prevBottom <= ty * 8 + 0.5f)) land = true;
    }
    if (land) { ny = ty * 8 - PH; pvy = 0; onGround = true; }
    for (int i = 0; i < nEnt && !land; i++) {
      Ent& e = ents[i];
      if (e.type != E_PLAT) continue;
      if (prevBottom <= e.y + 0.5f && bottom >= e.y && px + PW > e.x && px < e.x + 24) { ny = e.y - PH; pvy = 0; onGround = true; ridePlat = i; land = true; }
    }
    if (!land) ridePlat = -1;
  } else if (pvy < 0 && solidBox(px, ny, PW, PH)) {
    ny = floorf(ny / 8) * 8 + 8; pvy = 0;
  }
  py = ny;
  if (ridePlat >= 0 && !onGround) ridePlat = -1;

  // hazards & pickups
  if (py > 66) { die(); return; }
  int x0 = (int)floorf(px / 8), x1 = (int)floorf((px + PW - 0.01f) / 8);
  int y0 = (int)floorf(py / 8), y1 = (int)floorf((py + PH - 0.01f) / 8);
  for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) {
    char t = tileAt(tx, ty);
    if (t == '^' && boxHit(px + 1, py + 2, PW - 2, PH - 2, tx * 8 + 1, ty * 8 + 4, 6, 4)) { die(); return; }
    if (t == 'v' && boxHit(px + 1, py, PW - 2, PH - 2, tx * 8 + 1, ty * 8, 6, 4)) { die(); return; }
    if (t == 'c') { L[ty][tx] = ' '; coins++; gSfx(2600, 10); }
    if (t == 'C' && (spawnX != tx * 8)) { spawnX = tx * 8; spawnY = ty * 8; gWin(); }
    if (t == 'G') {
      state = 3; stateT = 0; gWin();
      int id = world * NLEV + lev;
      uint8_t got = 0; for (int k = 0; k < 3; k++) if (fries[k].got) got |= 1 << k;
      sv.fry[id] |= got;
      uint16_t secs10 = (uint16_t)min<uint32_t>(65535, elapsed / 100);
      newBest = !sv.best[id] || secs10 < sv.best[id];
      if (newBest) sv.best[id] = secs10;
      if (sv.unlocked < NWORLD * NLEV && id + 1 >= sv.unlocked) sv.unlocked = id + 2;
      sv.coins += coins;
      gameSave("gTater", sv);
      gameSubmitScore(totalFries());
      return;
    }
  }
  for (int k = 0; k < 3; k++) if (!fries[k].got && boxHit(px, py, PW, PH, fries[k].x * 8, fries[k].y * 8, 8, 8)) {
    fries[k].got = true;
    static const ToneStep t[] = { { 1568, 60 }, { 2093, 60 }, { 2637, 120 } };
    if (S.gameSfx) buzzSeq(t, 3);
    gFlash(0xC8A000, 2, 120); gHap(30, 6);
  }
}

static void entsUpdate() {
  for (int i = 0; i < nEnt; i++) {
    Ent& e = ents[i];
    if (!e.alive) continue;
    if (e.type == E_PLAT) {
      e.x += e.vx;
      if (e.x > e.x1) { e.x = e.x1; e.vx = -e.vx; }
      if (e.x < e.x0) { e.x = e.x0; e.vx = -e.vx; }
      continue;
    }
    if (fabsf(e.x - px) > 120) continue;                        // sleep off-screen
    e.t++;
    if (e.type == E_HOP) {
      bool ground = solidBox(e.x, e.y + 1, 7, 6);
      if (ground && e.t % 80 == 0) { e.vy = -2.8f; e.vx = (px < e.x ? -0.8f : 0.8f); }
      if (ground && e.vy >= 0) e.vx *= 0.8f;
    }
    e.vy = min(e.vy + 0.22f, 3.0f);
    float nx = e.x + e.vx;
    if (solidBox(nx, e.y, 7, 6)) { e.vx = -e.vx; nx = e.x; }
    else if (e.type == E_WALK && !solidBox(nx + (e.vx > 0 ? 7 : -1), e.y + 7, 1, 1)) { e.vx = -e.vx; nx = e.x; }   // ledge
    e.x = nx;
    float ny = e.y + e.vy;
    if (solidBox(e.x, ny, 7, 6)) { if (e.vy > 0) ny = floorf((ny + 6) / 8) * 8 - 6; else ny = floorf(ny / 8) * 8 + 8; e.vy = 0; }
    e.y = ny;
    if (e.y > 70) { e.alive = false; continue; }
    if (state == 1 && boxHit(px, py, PW, PH, e.x, e.y, 7, 6)) {
      if (pvy > 0.3f && prevBottom <= e.y + 3) { e.alive = false; pvy = -3.2f; gSfx(600, 30); gHap(20, 5); coins += 2; }
      else die();
    }
  }
}

// ---- drawing
static void drawTile(char t, int sx, int sy, int tx, int ty) {
  switch (t) {
    case '#': {
      bool top = tileAt(tx, ty - 1) != '#';
      switch (world) {
        case 0:   // fields: dithered soil + grass tufts
          u8g2.drawBox(sx, sy, 8, 8);
          u8g2.setDrawColor(0);
          for (int y = top ? 3 : 1; y < 8; y += 2) for (int x = (y / 2 + tx) & 1; x < 8; x += 3) u8g2.drawPixel(sx + x, sy + y);
          if (top) { u8g2.drawPixel(sx + 2, sy + 1); u8g2.drawPixel(sx + 6, sy + 1); }
          u8g2.setDrawColor(1); break;
        case 1:   // caves: bricks
          u8g2.drawBox(sx, sy, 8, 8); u8g2.setDrawColor(0);
          u8g2.drawHLine(sx, sy + 3, 8); u8g2.drawHLine(sx, sy + 7, 8);
          u8g2.drawPixel(sx + ((ty & 1) ? 3 : 7), sy + 1); u8g2.drawPixel(sx + ((ty & 1) ? 7 : 3), sy + 5);
          u8g2.setDrawColor(1); break;
        case 2:   // sky: cloud blocks
          if (top) { u8g2.drawRBox(sx, sy + 1, 8, 7, 2); } else u8g2.drawBox(sx, sy, 8, 8);
          u8g2.setDrawColor(0); u8g2.drawPixel(sx + 3, sy + 4); u8g2.setDrawColor(1); break;
        default:  // factory: riveted plates
          u8g2.drawFrame(sx, sy, 8, 8); u8g2.drawPixel(sx + 2, sy + 2); u8g2.drawPixel(sx + 5, sy + 5);
          if (top) u8g2.drawHLine(sx, sy + 1, 8);
          break;
      }
    } break;
    case '=': u8g2.drawHLine(sx, sy, 8); u8g2.drawHLine(sx, sy + 1, 8); u8g2.drawPixel(sx + 1, sy + 2); u8g2.drawPixel(sx + 6, sy + 2); break;
    case '^': u8g2.drawTriangle(sx, sy + 8, sx + 4, sy + 8, sx + 2, sy + 3); u8g2.drawTriangle(sx + 4, sy + 8, sx + 8, sy + 8, sx + 6, sy + 3); break;
    case 'v': u8g2.drawTriangle(sx, sy, sx + 4, sy, sx + 2, sy + 5); u8g2.drawTriangle(sx + 4, sy, sx + 8, sy, sx + 6, sy + 5); break;
    case 'c': { int b = ((millis() / 200) + tx) & 1; u8g2.drawDisc(sx + 4, sy + 4 - b, 2); } break;
    case 'C': {
      u8g2.drawVLine(sx + 1, sy - 8, 16);
      bool on = spawnX == tx * 8;
      if (on) u8g2.drawTriangle(sx + 2, sy - 8, sx + 2, sy - 2, sx + 7, sy - 5);
      else    { u8g2.drawLine(sx + 2, sy - 8, sx + 6, sy - 5); u8g2.drawLine(sx + 6, sy - 5, sx + 2, sy - 2); }
    } break;
    case 'G': {
      u8g2.drawVLine(sx + 1, sy - 16, 24); u8g2.drawDisc(sx + 1, sy - 17, 1);
      int w = 4 + ((millis() / 150) & 1);
      u8g2.drawBox(sx + 2, sy - 16, w + 2, 6);
    } break;
  }
}

static void drawHud() {
  u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 128, 7); u8g2.setDrawColor(1);
  u8g2.setFont(F_TINY);
  char b[24]; snprintf(b, sizeof(b), "%d-%d", world + 1, lev + 1); u8g2.drawStr(1, 6, b);
  snprintf(b, sizeof(b), "o%d", coins); u8g2.drawStr(18, 6, b);
  for (int k = 0; k < 3; k++) { int x = 44 + k * 6; if (fries[k].got) u8g2.drawBox(x, 1, 4, 5); else u8g2.drawFrame(x, 1, 4, 5); }
  uint32_t s = elapsed / 1000;
  snprintf(b, sizeof(b), "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60)); u8g2.drawStr(127 - u8g2.getStrWidth(b), 6, b);
  if (deaths) { snprintf(b, sizeof(b), "x%d", deaths); u8g2.drawStr(78, 6, b); }
}

static void drawWorld() {
  int c = (int)cam;
  int tx0 = c / 8;
  for (int tx = tx0; tx <= tx0 + 16 && tx < lvlW; tx++)
    for (int ty = 0; ty < CH_H; ty++) {
      char t = L[ty][tx];
      if (t != ' ') drawTile(t, tx * 8 - c, ty * 8, tx, ty);
    }
  for (int k = 0; k < 3; k++) if (!fries[k].got) {
    int sx = fries[k].x * 8 - c, sy = fries[k].y * 8 + (((millis() / 250) & 1) ? 0 : -1);
    if (sx > -8 && sx < 128) spr8(sx, sy, SP_FRY, 8);
  }
  for (int i = 0; i < nEnt; i++) {
    const Ent& e = ents[i];
    if (!e.alive) continue;
    int sx = (int)e.x - c;
    if (sx < -24 || sx > 128) continue;
    if (e.type == E_PLAT) { for (int k = 0; k < 3; k++) drawTile('=', sx + k * 8, (int)e.y, 0, 0); u8g2.drawFrame(sx, (int)e.y, 24, 3); }
    else spr8(sx, (int)e.y - 2, e.type == E_WALK ? SP_WALK : SP_HOP, 8, e.vx > 0);
  }
  if (state != 2 || (stateT / 3) % 2) {
    const uint8_t* s = !onGround ? SP_JUMP : (fabsf(pvx) > 0.2f && ((millis() / 120) & 1)) ? SP_RUN : SP_IDLE;
    spr8o((int)px - 1 - c, (int)py - 1, s, 8, facingLeft);
  }
}

// ---- level select
static int selW = 0, selL = 0;
static void selectLoop() {
  if (btnLong(BTN_SELECT)) { gameReturnToHub(); return; }
  if (btnRepeat(BTN_RIGHT)) { if (selL < NLEV - 1) { selL++; gClick(); } }
  if (btnRepeat(BTN_LEFT))  { if (selL > 0) { selL--; gClick(); } }
  if (btnRepeat(BTN_DOWN))  { if (selW < NWORLD - 1) { selW++; gClick(); } }
  if (btnRepeat(BTN_UP))    { if (selW > 0) { selW--; gClick(); } }
  int id = selW * NLEV + selL;
  bool locked = id >= sv.unlocked;
  if (btnTap(BTN_SELECT)) { if (locked) gSfx(200, 60); else { world = selW; lev = selL; startLevel(); return; } }

  u8g2.clearBuffer();
  u8g2.setFont(F_UI); u8g2.drawStr(2, 10, "Tater Run");
  u8g2.setFont(F_TINY);
  char b[24]; snprintf(b, sizeof(b), "fries %d/%d", totalFries(), NWORLD * NLEV * 3); u8g2.drawStr(126 - u8g2.getStrWidth(b), 9, b);
  u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(F_SMALL);
  snprintf(b, sizeof(b), "World %d: %s", selW + 1, WORLD_NAME[selW]); u8g2.drawStr(2, 22, b);
  for (int i = 0; i < NLEV; i++) {
    int x = 3 + i * 21, y = 26, idd = selW * NLEV + i;
    bool sel = i == selL, lk = idd >= sv.unlocked;
    if (sel) u8g2.drawRBox(x, y, 18, 20, 2); else u8g2.drawRFrame(x, y, 18, 20, 2);
    u8g2.setDrawColor(sel ? 0 : 1);
    if (lk) { u8g2.drawFrame(x + 6, y + 8, 7, 5); u8g2.drawCircle(x + 9, y + 7, 2, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT); }
    else {
      snprintf(b, sizeof(b), "%d", i + 1); u8g2.drawStr(x + 7, y + 10, b);
      for (int k = 0; k < 3; k++) { int fx = x + 3 + k * 5; if (sv.fry[idd] & (1 << k)) u8g2.drawBox(fx, y + 14, 3, 3); else u8g2.drawPixel(fx + 1, y + 15); }
    }
    u8g2.setDrawColor(1);
  }
  u8g2.setFont(F_TINY);
  if (locked) u8g2.drawStr(2, 56, "locked: clear the level before");
  else if (sv.best[id]) { snprintf(b, sizeof(b), "best %u.%us", sv.best[id] / 10, sv.best[id] % 10); u8g2.drawStr(2, 56, b); }
  else u8g2.drawStr(2, 56, "not cleared yet");
  u8g2.drawStr(2, 63, "</> level  U/D world  SEL play");
  gfxFlush();
}

// ---- entry points
void taterEnter() { loadSave(); state = 0; int u = min<int>(sv.unlocked - 1, NWORLD * NLEV - 1); selW = u / NLEV; selL = u % NLEV; }

void taterLoop() {
  if (state == 0) { selectLoop(); return; }
  uint32_t now = millis();
  static const char* const EXTRA[] = { "Restart level", "Level select" };
  int pr = 0;
  if (btnTap(BTN_SELECT) && !tPause.open && state == 1) { tPause.open = true; tPause.sel = 0; gClick(); }
  else pr = tPause.update(EXTRA, 2);
  if (pr == 2) { gameReturnToHub(); return; }
  if (pr == 3) { startLevel(); return; }
  if (pr == 4) { state = 0; return; }
  if (tPause.open) { lastMs = now; }
  else if (state == 1) {
    elapsed += now - lastMs; lastMs = now;
    playerUpdate();
    if (state == 1) entsUpdate(); else { for (int i = 0; i < nEnt; i++) if (ents[i].type == E_PLAT) { ents[i].x += ents[i].vx; if (ents[i].x > ents[i].x1 || ents[i].x < ents[i].x0) ents[i].vx = -ents[i].vx; } }
    float target = px - 56 + (facingLeft ? -10 : 10);
    cam += (target - cam) * 0.18f;
    cam = clampv(cam, 0.0f, (float)(lvlW * 8 - 128));
  } else if (state == 2) {
    lastMs = now;
    if (++stateT > 24) { respawn(); state = 1; }
  } else if (state == 3) {
    lastMs = now; stateT++;
    if (stateT > 15 && btnTap(BTN_SELECT)) {
      int id = world * NLEV + lev + 1;
      if (id < NWORLD * NLEV) { world = id / NLEV; lev = id % NLEV; startLevel(); } else { state = 0; }
      return;
    }
    if (stateT > 15 && btnTap(BTN_LEFT)) { selW = world; selL = lev; state = 0; return; }
  }

  u8g2.clearBuffer();
  drawWorld();
  drawHud();
  if (state == 3) {
    u8g2.setDrawColor(0); u8g2.drawBox(14, 10, 100, 48); u8g2.setDrawColor(1); u8g2.drawRFrame(14, 10, 100, 48, 3);
    u8g2.setFont(F_MEDB); u8g2.drawStr(64 - u8g2.getStrWidth("CLEAR!") / 2, 24, "CLEAR!");
    u8g2.setFont(F_SMALL);
    char b[32]; int got = 0; for (int k = 0; k < 3; k++) got += fries[k].got;
    snprintf(b, sizeof(b), "%lu.%lus%s", (unsigned long)(elapsed / 1000), (unsigned long)(elapsed / 100 % 10), newBest ? "  best!" : "");
    u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 34, b);
    snprintf(b, sizeof(b), "fries %d/3  coins %d", got, coins); u8g2.drawStr(64 - u8g2.getStrWidth(b) / 2, 43, b);
    u8g2.setFont(F_TINY); u8g2.drawStr(64 - u8g2.getStrWidth("SEL next   < levels") / 2, 54, "SEL next   < levels");
  }
  tPause.draw("Paused", EXTRA, 2);
  gfxFlush();
}
