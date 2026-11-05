// Games hub: hi-scores, theme music and the in-game "Sound & LED" settings.
#include "Multitool.h"

static const MiniGame GAMES[] = {
  { "Spud Knight", knightEnter, knightLoop },
  { "Tater Run", taterEnter,   taterLoop   },
  { "Spud Defense", defenseEnter, defenseLoop },
  { "Snake",     snakeEnter,   snakeLoop   },
  { "Pong",      pongEnter,    pongLoop    },
  { "Tetris",    tetrisEnter,  tetrisLoop  },
  { "Flappy",    flappyEnter,  flappyLoop  },
  { "Dungeon",   dungeonEnter, dungeonLoop },
  { "Tilt Maze", tmazeEnter,   tmazeLoop   },
  { "Tilt Race", dodgeEnter,   dodgeLoop   },
  { "Blaster",   blasterEnter, blasterLoop },
  { "Sonar",     sonarEnter,   sonarLoop   },
};
static const int NGAMES = (int)ARRAY_LEN(GAMES);
static const char* HS_KEY[NGAMES] = { "hsKnight","hsTater","hsDefense","hsSnake","hsPong","hsTetris","hsFlappy","hsDungeon",
                                      "hsMaze","hsDodge","hsBlast","hsSonar" };
static const char* HS_TAG[NGAMES] = { "flr","fry","*","hi","hi","hi","hi","flr","lvl","hi","hi","hi" };

static int  s_sel = 0, s_top = 0;
static int  s_current = -1;       // -1 = hub / settings
static bool s_gset = false;       // showing the Sound & LED menu
static int  s_gsSel = 0, s_gsTop = 0;

// ---- music: every game has its own theme (built-in songs, Music.cpp)
// index into the built-in song list, same order as GAMES[]
static const uint8_t THEME_IDX[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

int         gamesCount()        { return NGAMES; }
const char* gameName(int i)     { return (i >= 0 && i < NGAMES) ? GAMES[i].name : ""; }
const char* gameHsKey(int i)    { return (i >= 0 && i < NGAMES) ? HS_KEY[i] : "hsX"; }

// ---- hi-scores
uint32_t hiscoreGet(const char* key)           { return prefs.getUInt(key, 0); }
void     hiscoreSet(const char* key, uint32_t v){ prefs.putUInt(key, v); }
static const char* keyForCurrent() { return (s_current >= 0 && s_current < NGAMES) ? HS_KEY[s_current] : "hsX"; }
void gameSubmitScore(uint32_t v) { const char* k = keyForCurrent(); if (v > hiscoreGet(k)) hiscoreSet(k, v); }

// ---- gated in-game feedback
void gSfx(uint16_t f, uint16_t d)                 { if (S.gameSfx) buzzTone(f, d); }
void gHap(uint16_t ms, uint8_t str)               { if (S.gameHaptics) motorPulseStr(ms, str); }
void gFlash(uint32_t c, uint8_t t, uint16_t p)    { if (S.gameNeopixel) npxFlash(c, t, p); }
void gBase(uint8_t mode, uint32_t c, uint8_t sp)  { if (S.gameNeopixel) npxSetBase((NpxMode)mode, c, sp); else npxOff(); }
void gClick() { if (S.gameSfx) buzzTone(2400, 8); if (S.gameHaptics) motorPulseStr(6, 2); }
void gWin() {
  if (S.gameSfx) { static const ToneStep s[] = {{523,90},{659,90},{784,150}}; buzzSeq(s, 3); }
  if (S.gameHaptics) motorPulseStr(30, 6);
  if (S.gameNeopixel) npxFlash(strip.Color(0, 200, 40), 2, 120);
}
void gLose() {
  if (S.gameSfx) { static const ToneStep s[] = {{330,110},{247,110},{165,220}}; buzzSeq(s, 3); }
  if (S.gameHaptics) motorPulseStr(60, 7);
  if (S.gameNeopixel) npxFlash(strip.Color(200, 0, 0), 3, 110);
}

// ---- configurable sensor -> motion mapping
static inline float axisOf(uint8_t i, const Vec3f& v) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
float gTiltH(const Vec3f& a) { float x = axisOf(S.gTiltAxisH % 3, a); return S.gTiltInvH ? -x : x; }
float gTiltV(const Vec3f& a) { float x = axisOf(S.gTiltAxisV % 3, a); return S.gTiltInvV ? -x : x; }
float gAimH(const Vec3f& g)  { float x = axisOf(S.gAimAxisH % 3, g);  return S.gAimInvH ? -x : x; }
float gAimV(const Vec3f& g)  { float x = axisOf(S.gAimAxisV % 3, g);  return S.gAimInvV ? -x : x; }

// ---- orientation-aware centered panel
void gfxCenterPanel(const char* title, const char* l1, const char* l2, const char* hint) {
  int W = u8g2.getDisplayWidth(), H = u8g2.getDisplayHeight();
  int w = W - 6; int h = 50; if (h > H - 6) h = H - 6;
  int x = (W - w) / 2, y = (H - h) / 2;
  u8g2.setDrawColor(0); u8g2.drawBox(x, y, w, h); u8g2.setDrawColor(1);
  u8g2.drawRFrame(x, y, w, h, 3);
  u8g2.setFont(F_MEDB); u8g2.drawStr(x + (w - u8g2.getStrWidth(title)) / 2, y + 15, title);
  u8g2.setFont(F_SMALL);
  if (l1 && l1[0]) u8g2.drawStr(x + (w - u8g2.getStrWidth(l1)) / 2, y + 28, l1);
  if (l2 && l2[0]) u8g2.drawStr(x + (w - u8g2.getStrWidth(l2)) / 2, y + 38, l2);
  u8g2.setFont(F_TINY);
  if (hint && hint[0]) u8g2.drawStr(x + (w - u8g2.getStrWidth(hint)) / 2, y + h - 3, hint);
}

// ---- lifecycle
void gameReturnToHub() {
  s_current = -1;
  musicStop();                         // theme plays only inside a game
  u8g2.setDisplayRotation(U8G2_R0);
  inputSetFlip(false);
  gfxResetShadow();
}
void gamesEnter() {
  g_customFrame = true;
  uiSetRotation(false);
  inputSetFlip(false);
  s_current = -1; s_gset = false;
  gfxResetShadow();
  npxSetBase(NPX_BREATHE, npxHSV(160, 255, 255), 3);   // menu ambient (no music)
}
void gamesExit() {
  g_customFrame = false;
  musicStop();
  uiSetRotation(S.rotate180);
}

// ---- Game Settings: Sound & LED + sensor->motion mapping
enum { GS_MUSIC, GS_SFX, GS_HAP, GS_LED,
       GS_TILTH, GS_TILTHI, GS_TILTV, GS_TILTVI,
       GS_AIMH, GS_AIMHI, GS_AIMV, GS_AIMVI, GS_N };

static const char* gsLabel(int i) {
  switch (i) {
    case GS_MUSIC:  return "Music";
    case GS_SFX:    return "Sound FX";
    case GS_HAP:    return "Haptics";
    case GS_LED:    return "LED FX";
    case GS_TILTH:  return "Tilt H axis";
    case GS_TILTHI: return "Tilt H inv";
    case GS_TILTV:  return "Tilt V axis";
    case GS_TILTVI: return "Tilt V inv";
    case GS_AIMH:   return "Aim H axis";
    case GS_AIMHI:  return "Aim H inv";
    case GS_AIMV:   return "Aim V axis";
    case GS_AIMVI:  return "Aim V inv";
  }
  return "";
}
static void gsOnOff(char* o, size_t n, bool b) { snprintf(o, n, "%s", b ? "On" : "Off"); }
static void gsValue(int i, char* o, size_t n) {
  static const char AX[3] = { 'X', 'Y', 'Z' };
  switch (i) {
    case GS_MUSIC:  gsOnOff(o, n, S.gameMusic); break;
    case GS_SFX:    gsOnOff(o, n, S.gameSfx); break;
    case GS_HAP:    gsOnOff(o, n, S.gameHaptics); break;
    case GS_LED:    gsOnOff(o, n, S.gameNeopixel); break;
    case GS_TILTH:  snprintf(o, n, "%c", AX[S.gTiltAxisH % 3]); break;
    case GS_TILTHI: gsOnOff(o, n, S.gTiltInvH); break;
    case GS_TILTV:  snprintf(o, n, "%c", AX[S.gTiltAxisV % 3]); break;
    case GS_TILTVI: gsOnOff(o, n, S.gTiltInvV); break;
    case GS_AIMH:   snprintf(o, n, "%c", AX[S.gAimAxisH % 3]); break;
    case GS_AIMHI:  gsOnOff(o, n, S.gAimInvH); break;
    case GS_AIMV:   snprintf(o, n, "%c", AX[S.gAimAxisV % 3]); break;
    case GS_AIMVI:  gsOnOff(o, n, S.gAimInvV); break;
  }
}
static void gsAdjust(int i, int dir) {
  switch (i) {
    case GS_MUSIC:  S.gameMusic = !S.gameMusic; break;
    case GS_SFX:    S.gameSfx = !S.gameSfx; break;
    case GS_HAP:    S.gameHaptics = !S.gameHaptics; break;
    case GS_LED:    S.gameNeopixel = !S.gameNeopixel; if (!S.gameNeopixel) npxOff(); break;
    case GS_TILTH:  S.gTiltAxisH = (uint8_t)((S.gTiltAxisH + dir + 3) % 3); break;
    case GS_TILTHI: S.gTiltInvH = !S.gTiltInvH; break;
    case GS_TILTV:  S.gTiltAxisV = (uint8_t)((S.gTiltAxisV + dir + 3) % 3); break;
    case GS_TILTVI: S.gTiltInvV = !S.gTiltInvV; break;
    case GS_AIMH:   S.gAimAxisH = (uint8_t)((S.gAimAxisH + dir + 3) % 3); break;
    case GS_AIMHI:  S.gAimInvH = !S.gAimInvH; break;
    case GS_AIMV:   S.gAimAxisV = (uint8_t)((S.gAimAxisV + dir + 3) % 3); break;
    case GS_AIMVI:  S.gAimInvV = !S.gAimInvV; break;
  }
  settingsSaveSoon();
}

static void gameSettingsLoop() {
  if (btnLong(BTN_SELECT)) { s_gset = false; fbBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_gsSel < GS_N - 1) { s_gsSel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_gsSel > 0) { s_gsSel--; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_RIGHT)) { gsAdjust(s_gsSel, +1); fbNav(); }
  if (btnRepeat(BTN_LEFT))  { gsAdjust(s_gsSel, -1); fbNav(); }
  if (btnTap(BTN_SELECT))   { gsAdjust(s_gsSel, +1); fbNav(); }

  int rows = uiListRows();
  if (s_gsSel < s_gsTop) s_gsTop = s_gsSel;
  if (s_gsSel >= s_gsTop + rows) s_gsTop = s_gsSel - rows + 1;
  u8g2.clearBuffer();
  uiHeader("Game Settings");
  for (int r = 0; r < rows; r++) {
    int i = s_gsTop + r; if (i >= GS_N) break;
    char v[8]; gsValue(i, v, sizeof(v));
    uiListRow(r, gsLabel(i), v, i == s_gsSel);
  }
  uiScrollbar(GS_N, s_gsTop, rows);
  gfxFlush();
}

void gamesLoop() {
  if (s_current >= 0) { powerKeepAwake(2); GAMES[s_current].loop(); return; }
  if (s_gset) { gameSettingsLoop(); return; }

  // ---- hub (games + Sound&LED)
  int nRows = NGAMES + 1;
  if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_sel < nRows - 1) { s_sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_sel > 0) { s_sel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT)) {
    if (s_sel < NGAMES) {
      s_current = s_sel; gfxResetShadow(); GAMES[s_current].enter();
      if (s_current < (int)ARRAY_LEN(THEME_IDX)) songPlayRtttl(builtinRtttl(THEME_IDX[s_current]), true, true);
      fbSelect();
    }
    else { s_gset = true; s_gsSel = 0; s_gsTop = 0; fbSelect(); }
    return;
  }

  int rows = uiListRows();
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + rows) s_top = s_sel - rows + 1;

  u8g2.clearBuffer();
  uiHeader("Games");
  for (int r = 0; r < rows; r++) {
    int i = s_top + r;
    if (i > NGAMES) break;
    if (i == NGAMES) uiListRow(r, "Settings", ">", i == s_sel);
    else {
      char val[12];
      snprintf(val, sizeof(val), "%s %lu", HS_TAG[i], (unsigned long)hiscoreGet(HS_KEY[i]));
      uiListRow(r, GAMES[i].name, val, i == s_sel);
    }
  }
  uiScrollbar(nRows, s_top, rows);
  gfxFlush();
}
