// Tiny sprite toolkit for the games
//
// Sprites are written as ASCII art and packed into bytes at compile time:
//     static const uint8_t HERO[8] = { R8("..XXXX.."), R8(".X....X."), ... };
// 'X' (or '#') = lit pixel, anything else = transparent. Bit 7 = leftmost.
// 16-px wide rows use R16 and spr16().
#pragma once
#include <Arduino.h>

constexpr uint8_t R8(const char* s) {
  uint8_t v = 0;
  for (int i = 0; i < 8 && s[i]; i++) v |= (s[i] == 'X' || s[i] == '#') ? (uint8_t)(0x80 >> i) : 0;
  return v;
}
constexpr uint16_t R16(const char* s) {
  uint16_t v = 0;
  for (int i = 0; i < 16 && s[i]; i++) v |= (s[i] == 'X' || s[i] == '#') ? (uint16_t)(0x8000 >> i) : 0;
  return v;
}

// draw an 8-wide sprite (h rows); flipX mirrors it; color 1 = lit, 0 = erase
static inline void spr8(int x, int y, const uint8_t* rows, int h, bool flipX = false, uint8_t color = 1) {
  u8g2.setDrawColor(color);
  for (int r = 0; r < h; r++) {
    uint8_t b = rows[r];
    if (!b) continue;
    for (int c = 0; c < 8; c++) if (b & (0x80 >> (flipX ? 7 - c : c))) u8g2.drawPixel(x + c, y + r);
  }
  u8g2.setDrawColor(1);
}
static inline void spr16(int x, int y, const uint16_t* rows, int h, bool flipX = false, uint8_t color = 1) {
  u8g2.setDrawColor(color);
  for (int r = 0; r < h; r++) {
    uint16_t b = rows[r];
    if (!b) continue;
    for (int c = 0; c < 16; c++) if (b & (0x8000 >> (flipX ? 15 - c : c))) u8g2.drawPixel(x + c, y + r);
  }
  u8g2.setDrawColor(1);
}
// sprite with a 1-px dark outline so it reads on top of busy backgrounds
static inline void spr8o(int x, int y, const uint8_t* rows, int h, bool flipX = false) {
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) spr8(x + dx, y + dy, rows, h, flipX, 0);
  spr8(x, y, rows, h, flipX, 1);
}

static inline bool boxHit(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// tiny deterministic RNG (levels from seeds must be identical every time)
struct GRng {
  uint32_t s;
  explicit GRng(uint32_t seed = 1) : s(seed ? seed : 1) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }   // inclusive
  bool chance(int pct) { return (int)(next() % 100) < pct; }
};

// persistent game saves (NVS blobs, keys <= 15 chars). Versioned by size.
template <typename T> static inline bool gameLoad(const char* key, T& out) {
  if (prefs.getBytesLength(key) != sizeof(T)) return false;
  return prefs.getBytes(key, &out, sizeof(T)) == sizeof(T);
}
template <typename T> static inline void gameSave(const char* key, const T& v) { prefs.putBytes(key, &v, sizeof(T)); }

// pause menu shared by the new games (each game picks its own key to open it): returns 0 none, 1 resume, 2 quit, 3+ extra items
struct PauseMenu {
  bool open = false; int sel = 0;
  int update(const char* const* extra = nullptr, int nExtra = 0) {
    if (!open) return 0;
    int n = 2 + nExtra;
    if (btnRepeat(BTN_DOWN)) { sel = (sel + 1) % n; gClick(); }
    if (btnRepeat(BTN_UP))   { sel = (sel + n - 1) % n; gClick(); }
    if (btnTap(BTN_LEFT))    { open = false; return 1; }
    if (btnTap(BTN_SELECT))  { open = false; return sel == 0 ? 1 : sel == 1 ? 2 : 3 + (sel - 2); }
    return 0;
  }
  void draw(const char* title, const char* const* extra = nullptr, int nExtra = 0) {
    if (!open) return;
    int n = 2 + nExtra, h = 18 + n * 10;
    int y0 = (64 - h) / 2;
    u8g2.setDrawColor(0); u8g2.drawBox(18, y0, 92, h); u8g2.setDrawColor(1);
    u8g2.drawRFrame(18, y0, 92, h, 3);
    u8g2.setFont(F_UI); u8g2.drawStr(64 - u8g2.getStrWidth(title) / 2, y0 + 11, title);
    u8g2.setFont(F_SMALL);
    for (int i = 0; i < n; i++) {
      const char* t = i == 0 ? "Resume" : i == 1 ? "Quit" : extra[i - 2];
      int y = y0 + 22 + i * 10;
      if (i == sel) { u8g2.drawBox(22, y - 8, 84, 10); u8g2.setDrawColor(0); }
      u8g2.drawStr(64 - u8g2.getStrWidth(t) / 2, y, t);
      u8g2.setDrawColor(1);
    }
  }
};
