// Debounced 5-way D-pad with edges, long-press and auto-repeat
//
// Call inputUpdate() once per loop, then query with the helpers below.
//   btnPressed  : fired on the frame the button goes down (snappy actions)
//   btnTap      : fired on release if it was a short press & no long fired
//                 (use when the same button also has a long-press action)
//   btnLong     : fired once when the hold crosses the long-press threshold
//                 (SELECT: on release after a long hold, see the power gesture)
//   btnRepeat   : press edge + auto-repeat while held (list navigation)
//   btnDown     : raw debounced level
#pragma once
#include <Arduino.h>

enum Btn : uint8_t {
  BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SELECT, BTN_COUNT
};

void inputBegin();
void inputUpdate();

bool     btnDown(uint8_t b);
bool     btnPressed(uint8_t b);
bool     btnReleased(uint8_t b);
bool     btnTap(uint8_t b);       // short release, suppressed if long fired
bool     btnLong(uint8_t b);      // one-shot at threshold
bool     btnRepeat(uint8_t b);    // edge + auto-repeat
uint32_t btnHeldMs(uint8_t b);

bool     anyButtonDown();
uint32_t lastActivityMs();        // millis() of most recent button change
void     inputMarkActivity();     // bump activity (used by sensors/airmouse)

// Suppress all events for a button until it is physically released again.
// Used on screen transitions so a held SELECT doesn't "leak" its release
// (tap) into the screen we just opened.
void     btnConsume(uint8_t b);
// Drop every event of the current frame and swallow all held buttons until
// released (used when an alarm/ring overlay eats the press that silences it).
void     inputClearFrame();

// Power gesture: keep holding SELECT past the "back" point (~1.5 s) and the
// screen turns off instead (back is only taken if you let go before that).
bool     btnPowerGesture();       // fired once when the hold crosses BTN_POWER_MS
float    selectHoldProgress();    // 0 = not in the back/sleep zone, else 0..1 towards sleep
// Pointer overlay: SELECT acts as a plain mouse button (no back / power).
void     inputSetRawSelect(bool raw);
bool     inputRawSelect();

// When the display is flipped 180, swap UP<->DOWN and LEFT<->RIGHT so the
// D-pad matches what the user sees. Called by uiSetRotation().
void     inputSetFlip(bool flipped);
