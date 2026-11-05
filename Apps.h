// Tiny application framework (table + navigation stack)
#pragma once
#include <Arduino.h>

enum AppId : uint8_t {
  APP_MENU = 0,
  APP_BATTERY,
  APP_IR,
  APP_MOTION,
  APP_LIGHT,
  APP_COMPASS,
  APP_SCALE,
  APP_MOUSE,
  APP_TRIP,
  APP_GAMES,
  APP_SETTINGS,
  APP_CONNECT,
  APP_MACRO,
  APP_PRESENTER,
  APP_SHUTTER,
  APP_SMART,
  APP_SMARTSTAT,
  APP_CLOCK,
  APP_POMODORO,
  APP_PCMON,
  APP_GUARD,
  APP_INBOX,
  APP_WIFISCAN,
  APP_BLESCAN,
  APP_LANSCAN,
  APP_DOCTOR,
  APP_WAKE,
  APP_POWER,
  APP_MUSIC,
  APP_COUNT
};

// Main-menu folders. The menu shows a carousel of the non-empty ones; a folder
// with a single app opens that app directly.
enum AppCat : uint8_t {
  CAT_NONE = 0,     // not listed (the menu itself)
  CAT_TOOLS,        // sensor tools
  CAT_CONTROL,      // HID: air mouse, macro pad, presenter...
  CAT_WIRELESS,     // WiFi / BLE / LAN toolkit
  CAT_SMART,        // smart button, IoT
  CAT_DESK,         // clock, pomodoro, PC monitor
  CAT_GAMES,
  CAT_SYSTEM,       // connect, settings, battery
  CAT_COUNT
};

// App flags (power behaviour)
#define APPF_LIVE   0x01    // keeps working with the screen off (buttons still act)
#define APPF_DIMOK  0x02    // a "look at it" screen: dims at the timeout, off at 4x
#define APPF_STAYON 0x04    // never times out while open

struct App {
  const char* name;
  uint8_t     glyph;        // GlyphId
  uint8_t     cat;          // AppCat
  void (*onEnter)();
  void (*onLoop)();
  void (*onExit)();
  uint8_t     flags;        // APPF_*
};

extern const App APPS[APP_COUNT];

void    appsBegin();
void    openApp(uint8_t id);   // push + enter
void    goBack();              // pop to previous (menu is the floor)
uint8_t currentApp();
void    appsDispatch();        // run current app's onLoop

void    startupSequence();     // boot animation + self-test (Startup.cpp)
void    settingsOpenCategory(const char* name);   // jump into Settings > <name>

// Universal "back" gesture: hold SELECT and let go (frees the whole D-pad for
// apps). Holding on past ~1.5 s turns the screen off instead.
static inline bool wantBack() { return btnLong(BTN_SELECT); }

// ---- per-app entry points (defined in their own .cpp files)
void menuEnter();      void menuLoop();
void batteryEnter();   void batteryLoop();
void irEnter();        void irLoop();        void irExit();
void motionEnter();    void motionLoop();
void lightEnter();     void lightLoop();     void lightExit();
void compassEnter();   void compassLoop();
void scaleEnter();     void scaleLoop();
void mouseEnter();     void mouseLoop();     void mouseExit();
void tripEnter();      void tripLoop();      void tripExit();
void gamesEnter();     void gamesLoop();     void gamesExit();
void settingsAppEnter(); void settingsAppLoop();
void connectEnter();   void connectLoop();   void connectExit();
void macroEnter();     void macroLoop();     void macroExit();
void presenterEnter(); void presenterLoop(); void presenterExit();
void shutterEnter();   void shutterLoop();   void shutterExit();
void smartEnter();     void smartLoop();
void smartStatusEnter(); void smartStatusLoop();
void clockEnter();     void clockLoop();     void clockExit();
void pomEnter();       void pomLoop();
void pcmonEnter();     void pcmonLoop();
void guardEnter();     void guardLoop();     void guardExit();
void inboxEnter();     void inboxLoop();     void inboxExit();
void wifiScanEnter();  void wifiScanLoop();  void wifiScanExit();
void bleScanEnter();   void bleScanLoop();   void bleScanExit();
void lanScanEnter();   void lanScanLoop();   void lanScanExit();
void doctorEnter();    void doctorLoop();    void doctorExit();
void wakeEnter();      void wakeLoop();      void wakeExit();
void powerAppEnter();  void powerAppLoop();
void musicEnter();     void musicLoop();     void musicExit();
