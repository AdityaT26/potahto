// D-pad debouncing, taps, long presses and auto-repeat
#include "Multitool.h"

static const uint8_t PINS[BTN_COUNT] = {
  PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_SELECT
};

static bool     s_stable[BTN_COUNT];
static bool     s_lastRaw[BTN_COUNT];
static uint32_t s_lastChange[BTN_COUNT];
static uint32_t s_downAt[BTN_COUNT];
static uint32_t s_nextRepeat[BTN_COUNT];
static bool     s_longFired[BTN_COUNT];

static bool s_pressed[BTN_COUNT], s_released[BTN_COUNT];
static bool s_tap[BTN_COUNT], s_long[BTN_COUNT], s_repeat[BTN_COUNT];
static bool s_suppress[BTN_COUNT];

static uint32_t s_lastActivity = 0;
static bool     s_flipped = false;
static bool     s_rawSelect = false;   // pointer overlay: SELECT is a plain mouse button
static bool     s_powerEv = false, s_powerFired = false;

// Map a logical (on-screen) button to the physical one, swapping the D-pad
// when the display is flipped 180 degrees. SELECT is unaffected.
static inline uint8_t rm(uint8_t b) {
  if (!s_flipped) return b;
  switch (b) {
    case BTN_UP:    return BTN_DOWN;
    case BTN_DOWN:  return BTN_UP;
    case BTN_LEFT:  return BTN_RIGHT;
    case BTN_RIGHT: return BTN_LEFT;
    default:        return b;
  }
}

static inline bool rawPressed(uint8_t b) {
  int v = digitalRead(PINS[b]);
#if BTN_ACTIVE_LOW
  return v == LOW;
#else
  return v == HIGH;
#endif
}

void inputBegin() {
  for (uint8_t b = 0; b < BTN_COUNT; b++) {
    pinMode(PINS[b], INPUT_PULLUP);
    s_stable[b] = false;
    s_lastRaw[b] = rawPressed(b);
    s_lastChange[b] = millis();
    s_longFired[b] = false;
    s_suppress[b] = false;
  }
  s_lastActivity = millis();
}

void inputUpdate() {
  uint32_t now = millis();
  s_powerEv = false;
  for (uint8_t b = 0; b < BTN_COUNT; b++) {
    s_pressed[b] = s_released[b] = s_tap[b] = s_long[b] = s_repeat[b] = false;

    bool raw = rawPressed(b);
    if (raw != s_lastRaw[b]) { s_lastRaw[b] = raw; s_lastChange[b] = now; }

    if ((now - s_lastChange[b]) >= BTN_DEBOUNCE_MS && s_stable[b] != raw) {
      s_stable[b] = raw;
      s_lastActivity = now;
      if (raw) {                                   // ---- pressed ----
        s_pressed[b] = true;
        s_repeat[b]  = true;                        // press = first repeat
        s_downAt[b]  = now;
        s_longFired[b] = false;
        s_nextRepeat[b] = now + BTN_REPEAT_DELAY_MS;
      } else {                                      // ---- released ----
        s_released[b] = true;
        uint32_t held = now - s_downAt[b];
        if (!s_longFired[b] && held < BTN_LONGPRESS_MS)
          s_tap[b] = true;
        // SELECT's long press ("back") fires on RELEASE, so holding on to it
        // can become the power gesture (screen off) instead of going back.
        if (b == BTN_SELECT && !s_rawSelect && held >= BTN_LONGPRESS_MS && !s_powerFired) s_long[b] = true;
        if (b == BTN_SELECT) s_powerFired = false;
      }
    }

    if (s_stable[b]) {                              // ---- held ----
      if (b == BTN_SELECT && !s_rawSelect) {
        if (!s_longFired[b] && (now - s_downAt[b]) >= BTN_LONGPRESS_MS) { s_longFired[b] = true; s_lastActivity = now; }
        if (!s_powerFired && (now - s_downAt[b]) >= BTN_POWER_MS) { s_powerFired = true; s_powerEv = true; }
      } else if (!s_longFired[b] && (now - s_downAt[b]) >= BTN_LONGPRESS_MS) {
        s_longFired[b] = true;
        s_long[b] = b != BTN_SELECT || !s_rawSelect;
        s_lastActivity = now;
      }
      if ((int32_t)(now - s_nextRepeat[b]) >= 0) {
        s_repeat[b] = true;
        s_nextRepeat[b] += BTN_REPEAT_RATE_MS;
        s_lastActivity = now;
      }
    }

    if (s_suppress[b]) {                            // swallow events until release
      s_pressed[b] = s_tap[b] = s_long[b] = s_repeat[b] = false;
      if (!s_stable[b]) s_suppress[b] = false;
    }
  }
}

void inputClearFrame() {
  for (uint8_t b = 0; b < BTN_COUNT; b++) {
    s_pressed[b] = s_released[b] = s_tap[b] = s_long[b] = s_repeat[b] = false;
    if (s_stable[b]) s_suppress[b] = true;
  }
}
void btnConsume(uint8_t b) { b = rm(b); if (b < BTN_COUNT) s_suppress[b] = true; }
void inputSetFlip(bool flipped) { s_flipped = flipped; }

bool     btnDown(uint8_t b)     { b = rm(b); return b < BTN_COUNT && s_stable[b]; }
bool     btnPressed(uint8_t b)  { b = rm(b); return b < BTN_COUNT && s_pressed[b]; }
bool     btnReleased(uint8_t b) { b = rm(b); return b < BTN_COUNT && s_released[b]; }
bool     btnTap(uint8_t b)      { b = rm(b); return b < BTN_COUNT && s_tap[b]; }
bool     btnLong(uint8_t b)     { b = rm(b); return b < BTN_COUNT && s_long[b]; }
bool     btnRepeat(uint8_t b)   { b = rm(b); return b < BTN_COUNT && s_repeat[b]; }
uint32_t btnHeldMs(uint8_t b)   { b = rm(b); return (b < BTN_COUNT && s_stable[b]) ? (millis() - s_downAt[b]) : 0; }

bool anyButtonDown() {
  for (uint8_t b = 0; b < BTN_COUNT; b++) if (s_stable[b]) return true;
  return false;
}
uint32_t lastActivityMs()  { return s_lastActivity; }
bool     btnPowerGesture() { return s_powerEv; }
void     inputSetRawSelect(bool raw) { s_rawSelect = raw; }
bool     inputRawSelect() { return s_rawSelect; }
float    selectHoldProgress() {
  if (s_rawSelect || !s_stable[BTN_SELECT] || s_powerFired) return 0;
  uint32_t h = millis() - s_downAt[BTN_SELECT];
  if (h < BTN_LONGPRESS_MS) return 0;
  return clampv((float)(h - BTN_LONGPRESS_MS) / (BTN_POWER_MS - BTN_LONGPRESS_MS), 0.0f, 1.0f);
}
void     inputMarkActivity() { s_lastActivity = millis(); }
