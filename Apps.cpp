// Application table + navigation stack
#include "Multitool.h"

const App APPS[APP_COUNT] = {
  //  name          glyph           folder        enter             loop             exit          flags
  { "Potahto",   GLYPH_TOOL,     CAT_NONE,     menuEnter,        menuLoop,        nullptr,      0 },
  { "Battery",   GLYPH_BATTERY,  CAT_SYSTEM,   batteryEnter,     batteryLoop,     nullptr,      APPF_DIMOK },
  { "IR Remote", GLYPH_IR,       CAT_TOOLS,    irEnter,          irLoop,          irExit,       0 },
  { "Motion",    GLYPH_MOTION,   CAT_TOOLS,    motionEnter,      motionLoop,      nullptr,      APPF_DIMOK },
  { "Light",     GLYPH_LIGHT,    CAT_TOOLS,    lightEnter,       lightLoop,       lightExit,    APPF_DIMOK | APPF_LIVE },
  { "Compass",   GLYPH_COMPASS,  CAT_TOOLS,    compassEnter,     compassLoop,     nullptr,      APPF_DIMOK },
  { "Scale",     GLYPH_SCALE,    CAT_TOOLS,    scaleEnter,       scaleLoop,       nullptr,      APPF_DIMOK },
  { "Air Mouse", GLYPH_MOUSE,    CAT_CONTROL,  mouseEnter,       mouseLoop,       mouseExit,    APPF_LIVE },
  { "Tripwire",  GLYPH_TRIP,     CAT_TOOLS,    tripEnter,        tripLoop,        tripExit,     0 },
  { "Games",     GLYPH_GAMES,    CAT_GAMES,    gamesEnter,       gamesLoop,       gamesExit,    0 },
  { "Settings",  GLYPH_SETTINGS, CAT_SYSTEM,   settingsAppEnter, settingsAppLoop, nullptr,      0 },
  { "Connect",   GLYPH_CONNECT,  CAT_SYSTEM,   connectEnter,     connectLoop,     connectExit,  APPF_DIMOK },
  { "Macro Pad", GLYPH_KEYS,     CAT_CONTROL,  macroEnter,       macroLoop,       macroExit,    APPF_LIVE },
  { "Presenter", GLYPH_SLIDES,   CAT_CONTROL,  presenterEnter,   presenterLoop,   presenterExit,APPF_LIVE },
  { "Shutter",   GLYPH_CAMERA,   CAT_CONTROL,  shutterEnter,     shutterLoop,     shutterExit,  APPF_LIVE },
  { "Smart Button", GLYPH_HOME,  CAT_SMART,    smartEnter,       smartLoop,       nullptr,      APPF_LIVE },
  { "Smart Status", GLYPH_INFO,  CAT_SMART,    smartStatusEnter, smartStatusLoop, nullptr,      0 },
  { "Clock",     GLYPH_CLOCK,    CAT_DESK,     clockEnter,       clockLoop,       clockExit,    APPF_DIMOK },
  { "Pomodoro",  GLYPH_HOURGLASS,CAT_DESK,     pomEnter,         pomLoop,         nullptr,      APPF_DIMOK },
  { "PC Monitor",GLYPH_CHART,    CAT_DESK,     pcmonEnter,       pcmonLoop,       nullptr,      APPF_DIMOK },
  { "Desk Guard",GLYPH_SHIELD,   CAT_DESK,     guardEnter,       guardLoop,       guardExit,    0 },
  { "Inbox",     GLYPH_MAIL,     CAT_DESK,     inboxEnter,       inboxLoop,       inboxExit,    0 },
  { "WiFi Scan", GLYPH_WIFI,     CAT_WIRELESS, wifiScanEnter,    wifiScanLoop,    wifiScanExit, APPF_DIMOK },
  { "BLE Scan",  GLYPH_BT,       CAT_WIRELESS, bleScanEnter,     bleScanLoop,     bleScanExit,  APPF_DIMOK },
  { "LAN Scan",  GLYPH_LAN,      CAT_WIRELESS, lanScanEnter,     lanScanLoop,     lanScanExit,  0 },
  { "Net Doctor",GLYPH_PULSE,    CAT_WIRELESS, doctorEnter,      doctorLoop,      doctorExit,   0 },
  { "Wake PC",   GLYPH_POWER,    CAT_WIRELESS, wakeEnter,        wakeLoop,        wakeExit,     0 },
  { "Power",     GLYPH_POWER,    CAT_SYSTEM,   powerAppEnter,    powerAppLoop,    nullptr,      0 },
  { "Music",     GLYPH_MUSIC,    CAT_TOOLS,    musicEnter,       musicLoop,       musicExit,    0 },
};

static uint8_t s_stack[8];
static uint8_t s_depth = 0;

uint8_t currentApp() { return s_stack[s_depth]; }

void appsBegin() {
  s_depth = 0;
  s_stack[0] = APP_MENU;
  if (APPS[APP_MENU].onEnter) APPS[APP_MENU].onEnter();
}
void openApp(uint8_t id) {
  if (id >= APP_COUNT) return;
  if (APPS[currentApp()].onExit) APPS[currentApp()].onExit();
  if (s_depth < 7) s_depth++;
  s_stack[s_depth] = id;
  if (APPS[id].onEnter) APPS[id].onEnter();
}
void goBack() {
  if (APPS[currentApp()].onExit) APPS[currentApp()].onExit();
  if (s_depth > 0) s_depth--;
  if (APPS[currentApp()].onEnter) APPS[currentApp()].onEnter();
}
void appsDispatch() {
  if (APPS[currentApp()].onLoop) APPS[currentApp()].onLoop();
}
