// Mini-game framework shared declarations
//
// Games run in "custom frame" mode: they own clearBuffer -> draw -> gfxFlush()
// and their own timing. gfxFlush() transmits only the changed 8px tiles.
// A game exits back to the hub by calling gameReturnToHub().
#pragma once
#include <Arduino.h>

struct MiniGame {
  const char* name;
  void (*enter)();
  void (*loop)();
};

// per-game entry points
void knightEnter(); void knightLoop();    // action roguelite, persistent upgrades
void taterEnter();  void taterLoop();     // platformer, 4 worlds x 6 levels
void defenseEnter(); void defenseLoop();  // tower defense, 4 gardens x 20 waves
void snakeEnter();   void snakeLoop();
void pongEnter();    void pongLoop();
void tetrisEnter();  void tetrisLoop();
void flappyEnter();  void flappyLoop();
void dungeonEnter(); void dungeonLoop();
void tmazeEnter();   void tmazeLoop();     // accelerometer maze
void dodgeEnter();   void dodgeLoop();     // accelerometer endless racer
void blasterEnter(); void blasterLoop();   // gyro aim shooter
void sonarEnter();   void sonarLoop();      // ToF hand-distance paddle

// game list accessors (for the config protocol / web app)
int         gamesCount();
const char* gameName(int i);
const char* gameHsKey(int i);

// shared helpers (defined in AppGames.cpp)
uint32_t hiscoreGet(const char* key);
void     hiscoreSet(const char* key, uint32_t v);
void     gameSubmitScore(uint32_t v);              // record vs current game's best
void     gameReturnToHub();                        // leave a game -> game list

// gated in-game feedback (respect the Sound & Haptics toggles in game settings)
void     gSfx(uint16_t freq, uint16_t dur);
void     gHap(uint16_t ms, uint8_t str);
void     gFlash(uint32_t color, uint8_t times, uint16_t period);
void     gBase(uint8_t mode, uint32_t color, uint8_t speed);
void     gClick();                                  // tiny move/select tick
void     gWin();                                    // victory reaction
void     gLose();                                   // fail reaction

// configurable sensor->motion mapping (set in Games > Settings > Controls)
float    gTiltH(const Vec3f& a);   // accel horizontal
float    gTiltV(const Vec3f& a);   // accel vertical
float    gAimH(const Vec3f& g);    // gyro horizontal
float    gAimV(const Vec3f& g);    // gyro vertical
void     gfxCenterPanel(const char* title, const char* l1,
                        const char* l2, const char* hint);   // orientation-aware
