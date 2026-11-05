// Drawing toolkit + modal/toast overlays  (128x64, U8g2 full buffer)
//
// Contract:  apps never call clearBuffer()/sendBuffer().  The main loop owns
// the frame: it clears, runs the app (or a modal), overlays the toast, sends.
// Every helper here is careful to clip/truncate so nothing ever draws off the
// 128x64 panel.
#pragma once
#include <Arduino.h>
#include "Sensors.h"   // Vec3f

// Shared fonts (U8g2 built-ins) used across every screen
#define F_TINY   u8g2_font_4x6_tf
#define F_SMALL  u8g2_font_5x7_tf
#define F_UI     u8g2_font_6x12_tf
#define F_MEDB   u8g2_font_7x13B_tf
#define F_BIG    u8g2_font_logisoso24_tr
#define F_HUGE   u8g2_font_logisoso28_tn

// Procedural tool icons (drawn with primitives, always crisp)
enum GlyphId : uint8_t {
  GLYPH_BATTERY = 0, GLYPH_IR, GLYPH_MOTION, GLYPH_LIGHT, GLYPH_COMPASS,
  GLYPH_SCALE, GLYPH_MOUSE, GLYPH_TRIP, GLYPH_SETTINGS, GLYPH_INFO,
  GLYPH_TOOL, GLYPH_GAMES, GLYPH_WIFI, GLYPH_CONNECT, GLYPH_CLOCK,
  GLYPH_HOME, GLYPH_KEYS, GLYPH_SLIDES, GLYPH_CAMERA,
  GLYPH_HOURGLASS, GLYPH_CHART, GLYPH_SHIELD, GLYPH_MAIL,
  GLYPH_BT, GLYPH_LAN, GLYPH_PULSE, GLYPH_POWER, GLYPH_MUSIC, GLYPH_LOCK, GLYPH_COUNT
};

void uiBegin();

// ---- frame ownership (main loop only)
void uiFrameBegin();
void uiFrameEnd();

// ---- games: partial-update renderer
// Games draw into the full buffer then call gfxFlush(), which diffs against a
// shadow copy and transmits only the changed 8px tiles (updateDisplayArea),
// saving I2C bandwidth. Call gfxResetShadow() after any full-screen change
// (enter, rotation change) to force one full refresh.
void gfxResetShadow();
void gfxFlush();

// ---- rotation
void uiSetRotation(bool flip);      // apply flip immediately
void uiAutoRotate(float rollDeg);   // if S.autoRotate, flip on tilt (hysteresis)

// ---- text helpers
int  uiTextWidth(const char* s);
void uiTruncate(char* dst, size_t dstsz, const char* src, int maxW);
void uiTextClipped(int x, int y, const char* s, int maxW);   // left aligned
void uiCenter(int cy, const char* s);                        // horiz centered
void uiRight(int xr, int y, const char* s);                  // right aligned

// ---- chrome / widgets
void uiHeader(const char* title);            // title bar + WiFi/BLE/battery
void uiWifiIcon(int x, int y);               // 9x7 status icon (blinks while connecting)
void uiQR(int x, int y, int scale, const char* text);  // QR code, white quiet zone
void uiBatteryIcon(int x, int y);            // 13x7
void uiGlyph(uint8_t g, int x, int y, uint8_t size);   // size 16/24/32
void uiProgressBar(int x, int y, int w, int h, float frac);
void uiBigValue(int cx, int cy, const char* value, const char* unit);
void uiHint(const char* s);                  // one-line hint at very bottom
int  uiListRows();                           // rows that fit in the body
void uiListRow(int slot, const char* label, const char* value, bool sel);
void uiScrollbar(int count, int topIndex, int rows);   // right-edge indicator

// ---- toast (transient, non-blocking)
void toast(const char* msg);
void toastUpdate();          // overlay, called by main loop each frame

// ---- modal overlays (take over input + drawing while active)
bool modalActive();
void modalUpdate();          // called by main loop instead of the app
void modalConfirm(const char* title, const char* msg, void (*onYes)());
void modalInfo(const char* title, const char* msg);
void modalSlider(const char* title, int* value, int lo, int hi, int step,
                 const char* unit, void (*onChange)(int), void (*onDone)());
// pick one of up to 12 items (copied); onPick(index) on SELECT, nothing on back
void modalChoice(const char* title, const char* const* items, int n, int sel, void (*onPick)(int));
void modalClose();
