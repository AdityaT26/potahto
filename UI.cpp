// Drawing toolkit, procedural icons, toast + modal overlays
#include "Multitool.h"
#include <math.h>
#include "PotQR.h"

static bool s_flip = false;   // current display flip state

// ---- lifecycle
void uiBegin() {
  u8g2.begin();
  u8g2.setFontMode(1);              // transparent glyphs (needed for highlights)
  u8g2.setFontPosBaseline();
  uiSetRotation(S.rotate180);
}
void uiFrameBegin() { u8g2.clearBuffer(); }
void uiFrameEnd()   { u8g2.sendBuffer(); }

// ---- games: partial-update renderer
static uint8_t s_shadow[1024];
static bool    s_gfxFull = true;

void gfxResetShadow() { s_gfxFull = true; }

void gfxFlush() {
  uint8_t* buf = u8g2.getBufferPtr();
  int tw = u8g2.getBufferTileWidth();      // native tiles (16 for 128 wide)
  int th = u8g2.getBufferTileHeight();     // native tiles (8 for 64 tall)
  int bw = tw * 8;                         // bytes per page
  int len = bw * th;
  if (len > (int)sizeof(s_shadow)) len = sizeof(s_shadow);

  if (s_gfxFull) {                          // forced full refresh
    u8g2.sendBuffer();
    memcpy(s_shadow, buf, len);
    s_gfxFull = false;
    return;
  }
  int minX = bw, maxX = -1, minP = th, maxP = -1;
  for (int p = 0; p < th; p++) {
    uint8_t* r = buf + p * bw;
    uint8_t* s = s_shadow + p * bw;
    for (int x = 0; x < bw; x++) {
      if (r[x] != s[x]) {
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (p < minP) minP = p;
        if (p > maxP) maxP = p;
      }
    }
  }
  if (maxX < 0) return;                      // nothing changed -> send nothing
  int tx = minX / 8;
  u8g2.updateDisplayArea(tx, minP, (maxX / 8) - tx + 1, maxP - minP + 1);
  memcpy(s_shadow, buf, len);
}

void uiSetRotation(bool flip) {
  s_flip = flip;
  u8g2.setDisplayRotation(flip ? U8G2_R2 : U8G2_R0);
  inputSetFlip(flip);          // swap D-pad so buttons match the flipped view
}
void uiAutoRotate(float ang) {
  // ang = in-plane gravity angle: ~0 upright, ~180 upside-down, ~90 on-side.
  // Decide around the natural 90deg boundary with a hysteresis band so it
  // flips both ways reliably (the old 45/135 band left it stuck once flipped).
  if (!S.autoRotate) return;
  float a = fabsf(ang);
  const float margin = 20.0f;
  bool want = s_flip;
  if (a > 90.0f + margin)      want = true;
  else if (a < 90.0f - margin) want = false;
  if (want != s_flip) uiSetRotation(want);
}

// ---- text
int uiTextWidth(const char* s) { return u8g2.getStrWidth(s); }

void uiTruncate(char* dst, size_t dstsz, const char* src, int maxW) {
  strncpy(dst, src, dstsz - 1);
  dst[dstsz - 1] = 0;
  if (u8g2.getStrWidth(dst) <= maxW) return;
  int len = strlen(dst);
  while (len > 1) {
    dst[--len] = 0;
    char cand[40];
    snprintf(cand, sizeof(cand), "%s..", dst);
    if (u8g2.getStrWidth(cand) <= maxW) {
      strncpy(dst, cand, dstsz - 1);
      dst[dstsz - 1] = 0;
      return;
    }
  }
  dst[0] = 0;
}
void uiTextClipped(int x, int y, const char* s, int maxW) {
  char buf[40];
  uiTruncate(buf, sizeof(buf), s, maxW);
  u8g2.drawStr(x, y, buf);
}
void uiCenter(int cy, const char* s) {
  int w = u8g2.getStrWidth(s);
  u8g2.drawStr((SCR_W - w) / 2, cy, s);
}
void uiRight(int xr, int y, const char* s) {
  u8g2.drawStr(xr - u8g2.getStrWidth(s), y, s);
}

// ---- battery icon (13x7)
void uiBatteryIcon(int x, int y) {
  u8g2.drawFrame(x, y, 12, 7);
  u8g2.drawBox(x + 12, y + 2, 1, 3);            // terminal nub
  int pct = battPercent();
  int fw = (pct * 9 + 50) / 100;                // 0..9 px
  if (fw > 0) u8g2.drawBox(x + 1, y + 1, fw + 1, 5);
}

// ---- header
// 9x7 WiFi status: arcs when online (more arcs = stronger), "AP" in setup mode,
// blinking dot while connecting, nothing when the radio is off.
void uiWifiIcon(int x, int y) {
  NetState st = netState();
  if (netSetupAPActive()) { u8g2.setFont(F_TINY); u8g2.drawStr(x, y + 6, "AP"); return; }
  if (st == NET_OFF) return;
  int cx = x + 4, by = y + 6;
  if (st == NET_ONLINE) {
    int r = netRssi();
    int bars = r > -60 ? 3 : (r > -72 ? 2 : 1);
    u8g2.drawPixel(cx, by);
    if (bars >= 1) u8g2.drawCircle(cx, by, 2, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    if (bars >= 2) u8g2.drawCircle(cx, by, 4, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    if (bars >= 3) u8g2.drawCircle(cx, by, 6, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  } else if (st == NET_SCANNING || st == NET_CONNECTING) {
    if ((millis() / 400) & 1) { u8g2.drawPixel(cx, by); u8g2.drawCircle(cx, by, 4, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT); }
  } else {                                        // wanted but not connected
    u8g2.drawCircle(cx, by, 4, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawLine(cx - 3, by - 5, cx + 3, by);     // struck through
  }
}

void uiHeader(const char* title) {
  u8g2.setFont(F_UI);
  uiTextClipped(2, 9, title, 84);               // leaves room for status icons
  if (notifyUnread() > 0) {                       // tiny envelope = unread notifications
    u8g2.drawFrame(86, 2, 7, 5); u8g2.drawLine(86, 2, 89, 4); u8g2.drawLine(92, 2, 89, 4);
  }
  if (watchBusy()) {                              // tiny eye = tripwire / desk guard armed
    u8g2.drawCircle(81, 4, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawCircle(81, 2, 3, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    u8g2.drawPixel(81, 3);
  }
  uiBatteryIcon(113, 1);
  if (g_bleConnected) {                          // tiny "B" marker
    u8g2.setFont(F_TINY);
    u8g2.drawStr(106, 7, "B");
  }
  uiWifiIcon(94, 1);
  u8g2.drawHLine(0, 11, SCR_W);
}

// QR code: version 1-3 (<= 29 modules) so a 2x scale still fits the panel.
// Drawn dark-on-white with a 2-module quiet zone for reliable phone scanning.
void uiQR(int x, int y, int scale, const char* text) {
  static uint8_t buf[128];                       // >= qrcode_getBufferSize(3) = 106
  QRCode qr;
  // The vendored encoder does not check capacity (it overruns its stack buffer
  // if the text is too long for the version), so choose the version here from
  // the byte-mode capacity at ECC level L: v1 17, v2 32, v3 53 bytes.
  size_t len = strlen(text);
  int ver = len <= 17 ? 1 : len <= 32 ? 2 : len <= 53 ? 3 : 0;
  if (!ver) { u8g2.setFont(F_TINY); u8g2.drawStr(x, y + 8, "QR: text too long"); return; }
  qrcode_initText(&qr, buf, ver, 0 /*ECC_LOW*/, text);
  int quiet = 2 * scale;                         // 2 modules; 1 if it would not fit
  if (qr.size * scale + 2 * quiet > SCR_H) quiet = scale;
  int side = qr.size * scale + 2 * quiet;
  u8g2.setDrawColor(1);
  u8g2.drawBox(x, y, side, side);
  u8g2.setDrawColor(0);
  for (int my = 0; my < qr.size; my++)
    for (int mx = 0; mx < qr.size; mx++)
      if (qrcode_getModule(&qr, mx, my)) u8g2.drawBox(x + quiet + mx * scale, y + quiet + my * scale, scale, scale);
  u8g2.setDrawColor(1);
}

// ---- procedural glyphs
void uiGlyph(uint8_t g, int x, int y, uint8_t s) {
  int cx = x + s / 2, cy = y + s / 2;
  switch (g) {
    case GLYPH_BATTERY: {
      int bw = s * 3 / 4, bh = s / 2;
      int bx = cx - bw / 2, by = cy - bh / 2;
      u8g2.drawFrame(bx, by, bw, bh);
      u8g2.drawBox(bx + bw, by + bh / 3, 2, bh / 3);
      u8g2.drawBox(bx + 2, by + 2, bw / 2, bh - 4);
    } break;
    case GLYPH_IR: {
      int ex = x + s / 4, ey = y + 3 * s / 4;
      u8g2.drawDisc(ex, ey, s / 10 + 1);
      for (int i = 1; i <= 3; i++)
        u8g2.drawCircle(ex, ey, i * s / 6, U8G2_DRAW_UPPER_RIGHT);
    } break;
    case GLYPH_MOTION: {
      int r = s / 2 - 2;
      u8g2.drawCircle(cx, cy, r);
      u8g2.drawHLine(cx - r, cy, 2 * r);
      u8g2.drawVLine(cx, cy - r, 2 * r);
      u8g2.drawDisc(cx + s / 6, cy - s / 6, s / 8);
    } break;
    case GLYPH_LIGHT: {
      u8g2.drawDisc(cx, cy, s / 5);
      for (int a = 0; a < 8; a++) {
        float th = a * PI / 4;
        u8g2.drawLine(cx + cosf(th) * s / 3, cy + sinf(th) * s / 3,
                      cx + cosf(th) * (s / 2 - 1), cy + sinf(th) * (s / 2 - 1));
      }
    } break;
    case GLYPH_COMPASS: {
      u8g2.drawCircle(cx, cy, s / 2 - 1);
      u8g2.drawTriangle(cx, cy - s / 3, cx - s / 8 - 1, cy, cx + s / 8 + 1, cy);
      u8g2.drawTriangle(cx, cy + s / 3, cx - s / 8 - 1, cy, cx + s / 8 + 1, cy);
    } break;
    case GLYPH_SCALE: {
      int rw = s - 6, rh = s / 3, rx = cx - rw / 2, ry = cy - rh / 2;
      u8g2.drawFrame(rx, ry, rw, rh);
      for (int i = 1; i < 6; i++) {
        int tx = rx + i * rw / 6;
        int tl = (i & 1) ? rh / 2 : (rh * 3 / 4);
        u8g2.drawVLine(tx, ry, tl);
      }
    } break;
    case GLYPH_MOUSE: {
      int mw = s / 2, mh = s * 3 / 4, mx = cx - mw / 2, my = cy - mh / 2;
      u8g2.drawRFrame(mx, my, mw, mh, mw / 2);
      u8g2.drawVLine(cx, my + 2, mh / 3);
      u8g2.drawDisc(cx, my + mh / 6, 1);
    } break;
    case GLYPH_TRIP: {
      u8g2.drawBox(x + 2, y + 3, 2, s - 6);
      u8g2.drawBox(x + s - 4, y + 3, 2, s - 6);
      for (int i = x + 5; i < x + s - 5; i += 4) u8g2.drawPixel(i, cy);
      u8g2.drawDisc(cx, cy, 2);
    } break;
    case GLYPH_SETTINGS: {
      int r = s / 4;
      u8g2.drawCircle(cx, cy, r);
      u8g2.drawDisc(cx, cy, 2);
      for (int a = 0; a < 8; a++) {
        float th = a * PI / 4;
        u8g2.drawLine(cx + cosf(th) * r, cy + sinf(th) * r,
                      cx + cosf(th) * (s / 2 - 1), cy + sinf(th) * (s / 2 - 1));
      }
    } break;
    case GLYPH_INFO: {
      u8g2.drawCircle(cx, cy, s / 2 - 2);
      u8g2.drawDisc(cx, cy - s / 5, 1);
      u8g2.drawBox(cx - 1, cy - s / 12, 2, s / 3);
    } break;
    case GLYPH_GAMES: {
      int gw = s - 2, gh = s * 3 / 5, gx = cx - gw / 2, gy = cy - gh / 2;
      u8g2.drawRFrame(gx, gy, gw, gh, gh / 3);
      int px = gx + gw / 4;
      u8g2.drawHLine(px - 3, cy, 7); u8g2.drawVLine(px, cy - 3, 7);   // d-pad
      u8g2.drawDisc(gx + gw * 3 / 4 - 2, cy - 2, 1);                  // buttons
      u8g2.drawDisc(gx + gw * 3 / 4 + 2, cy + 2, 1);
    } break;
    case GLYPH_WIFI: {
      int by = y + s - 4;
      u8g2.drawDisc(cx, by, s / 12 + 1);
      for (int i = 1; i <= 3; i++)
        u8g2.drawCircle(cx, by, i * (s - 6) / 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    } break;
    case GLYPH_CONNECT: {                         // chain link
      int lw = s / 2 + 1, lh = s / 3 + 1, r = lh / 2;
      int ly = cy - lh / 2;
      u8g2.drawRFrame(x + 1, ly, lw, lh, r);
      u8g2.drawRFrame(x + s - 1 - lw, ly, lw, lh, r);
      u8g2.drawHLine(cx - s / 6, cy, s / 3 + 1);
    } break;
    case GLYPH_CLOCK: {
      int r = s / 2 - 2;
      u8g2.drawCircle(cx, cy, r);
      u8g2.drawLine(cx, cy, cx, cy - r * 2 / 3);
      u8g2.drawLine(cx, cy, cx + r / 2, cy + r / 4);
      u8g2.drawDisc(cx, cy, 1);
    } break;
    case GLYPH_HOME: {
      int hw = s * 2 / 3, hx = cx - hw / 2, roofY = y + s / 6, wallY = cy - s / 12;
      u8g2.drawLine(hx - 2, wallY, cx, roofY);
      u8g2.drawLine(cx, roofY, hx + hw + 1, wallY);
      u8g2.drawFrame(hx, wallY, hw, y + s - 3 - wallY);
      u8g2.drawBox(cx - s / 12, y + s - 3 - s / 4, s / 6 + 1, s / 4);
    } break;
    case GLYPH_KEYS: {                            // keyboard
      int kw = s - 2, kh = s / 2, kx = cx - kw / 2, ky = cy - kh / 2;
      u8g2.drawRFrame(kx, ky, kw, kh, 2);
      int step = kw / 5;
      for (int r = 0; r < 2; r++)
        for (int c = 0; c < 4; c++) u8g2.drawBox(kx + 3 + c * step, ky + 3 + r * (kh / 3), 2, 2);
      u8g2.drawHLine(kx + step, ky + kh - 4, kw - 2 * step);
    } break;
    case GLYPH_SLIDES: {                          // screen with a play arrow
      int sw = s - 4, sh = s * 3 / 5, sx = cx - sw / 2, sy = y + 2;
      u8g2.drawFrame(sx, sy, sw, sh);
      u8g2.drawTriangle(cx - s / 8, sy + sh / 4, cx - s / 8, sy + sh * 3 / 4, cx + s / 6, sy + sh / 2);
      u8g2.drawVLine(cx, sy + sh, s / 5);
      u8g2.drawHLine(cx - s / 5, y + s - 2, 2 * s / 5 + 1);
    } break;
    case GLYPH_CAMERA: {
      int bw = s - 4, bh = s / 2, bx = cx - bw / 2, by = cy - bh / 2 + 1;
      u8g2.drawRFrame(bx, by, bw, bh, 2);
      u8g2.drawBox(cx - s / 6, by - 2, s / 3, 2);
      u8g2.drawCircle(cx, by + bh / 2, bh / 3);
    } break;
    case GLYPH_HOURGLASS: {
      int hw = s / 2, top = y + 2, bot = y + s - 3;
      u8g2.drawHLine(cx - hw / 2 - 1, top, hw + 3); u8g2.drawHLine(cx - hw / 2 - 1, bot, hw + 3);
      u8g2.drawLine(cx - hw / 2, top, cx + hw / 2, bot); u8g2.drawLine(cx + hw / 2, top, cx - hw / 2, bot);
      u8g2.drawTriangle(cx - hw / 4, bot - 1, cx + hw / 4 + 1, bot - 1, cx, cy + 1);
    } break;
    case GLYPH_CHART: {
      int bw = s / 6, base = y + s - 3;
      int hs[4] = { s / 3, s / 2, s / 4, s * 2 / 3 };
      for (int i = 0; i < 4; i++) u8g2.drawBox(x + 2 + i * (bw + 2), base - hs[i], bw, hs[i]);
      u8g2.drawHLine(x + 1, base, s - 2);
    } break;
    case GLYPH_SHIELD: {
      int w = s * 2 / 3, l = cx - w / 2, r = cx + w / 2, t = y + 2, m = cy + s / 8, b = y + s - 2;
      u8g2.drawLine(l, t, r, t); u8g2.drawLine(l, t, l, m); u8g2.drawLine(r, t, r, m);
      u8g2.drawLine(l, m, cx, b); u8g2.drawLine(r, m, cx, b);
      u8g2.drawLine(cx - s / 8, cy, cx - 1, cy + s / 8); u8g2.drawLine(cx - 1, cy + s / 8, cx + s / 6, cy - s / 8);
    } break;
    case GLYPH_MAIL: {
      int w = s - 4, h = s / 2 + 1, l = cx - w / 2, t = cy - h / 2;
      u8g2.drawFrame(l, t, w, h);
      u8g2.drawLine(l, t, cx, t + h / 2); u8g2.drawLine(l + w - 1, t, cx, t + h / 2);
    } break;
    case GLYPH_BT: {                              // Bluetooth rune
      int t = y + 2, b = y + s - 3, w = s / 4;
      u8g2.drawVLine(cx, t, b - t + 1);
      u8g2.drawLine(cx, t, cx + w, t + w); u8g2.drawLine(cx + w, t + w, cx - w, b - w);
      u8g2.drawLine(cx, b, cx + w, b - w); u8g2.drawLine(cx + w, b - w, cx - w, t + w);
    } break;
    case GLYPH_LAN: {                             // three nodes on a bus
      int bw = s / 4, top = y + 2, bot = y + s - 3 - bw / 2;
      u8g2.drawFrame(cx - bw / 2, top, bw, bw);
      u8g2.drawVLine(cx, top + bw, cy - top - bw + 1);
      u8g2.drawHLine(x + 2 + bw / 2, cy, s - 4 - bw);
      u8g2.drawVLine(x + 2 + bw / 2, cy, bot - cy); u8g2.drawVLine(x + s - 3 - bw / 2, cy, bot - cy);
      u8g2.drawFrame(x + 2, bot, bw, bw); u8g2.drawFrame(x + s - 2 - bw, bot, bw, bw);
    } break;
    case GLYPH_PULSE: {                           // heartbeat line
      int l = x + 1, r = x + s - 2, m = cy;
      u8g2.drawLine(l, m, cx - s / 5, m); u8g2.drawLine(cx - s / 5, m, cx - s / 10, y + 3);
      u8g2.drawLine(cx - s / 10, y + 3, cx + s / 10, y + s - 4); u8g2.drawLine(cx + s / 10, y + s - 4, cx + s / 5, m);
      u8g2.drawLine(cx + s / 5, m, r, m);
    } break;
    case GLYPH_POWER: {
      int r = s / 2 - 3;
      u8g2.drawCircle(cx, cy + 1, r);
      u8g2.setDrawColor(0); u8g2.drawBox(cx - 2, y, 5, cy - y); u8g2.setDrawColor(1);
      u8g2.drawVLine(cx, y + 1, s / 2);
    } break;
    case GLYPH_MUSIC: {                           // two beamed eighth notes
      int r = max(2, s / 8), x1 = x + s / 4, x2 = x + s * 3 / 4 - 1, yb = y + s * 3 / 4;
      u8g2.drawDisc(x1, yb, r); u8g2.drawDisc(x2, yb - s / 8, r);
      u8g2.drawVLine(x1 + r, y + s / 5, yb - y - s / 5); u8g2.drawVLine(x2 + r, y + s / 5 - s / 8 + 1, yb - y - s / 5);
      u8g2.drawLine(x1 + r, y + s / 5, x2 + r, y + s / 5 - s / 8 + 1);
      u8g2.drawLine(x1 + r, y + s / 5 + 1, x2 + r, y + s / 5 - s / 8 + 2);
    } break;
    case GLYPH_LOCK: {
      int w = s / 2 + 2, hgt = s / 3 + 1, bx = cx - w / 2, by = cy;
      u8g2.drawRFrame(bx, by - 1, w, hgt + 1, 2);
      u8g2.drawCircle(cx, by - 1, w / 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
      u8g2.drawVLine(cx - w / 3, by - 1, 1); u8g2.drawVLine(cx + w / 3, by - 1, 1);
      u8g2.drawBox(cx - 1, by + hgt / 3, 2, hgt / 3 + 1);
    } break;
    case GLYPH_TOOL:
    default: {
      u8g2.drawLine(x + 4, y + s - 4, x + s - 6, y + 6);
      u8g2.drawLine(x + 5, y + s - 4, x + s - 5, y + 6);
      u8g2.drawCircle(x + s - 6, y + 6, 3);
      u8g2.drawDisc(x + 4, y + s - 4, 2);
    } break;
  }
}

// ---- misc widgets
void uiProgressBar(int x, int y, int w, int h, float frac) {
  frac = clampv(frac, 0.0f, 1.0f);
  u8g2.drawFrame(x, y, w, h);
  int iw = (int)((w - 4) * frac);
  if (iw > 0) u8g2.drawBox(x + 2, y + 2, iw, h - 4);
}
void uiBigValue(int cx, int cy, const char* value, const char* unit) {
  u8g2.setFont(F_BIG);
  int vw = u8g2.getStrWidth(value);
  u8g2.setFont(F_SMALL);
  int uw = unit ? u8g2.getStrWidth(unit) : 0;
  int total = vw + (uw ? uw + 3 : 0);
  int sx = cx - total / 2;
  u8g2.setFont(F_BIG);
  u8g2.drawStr(sx, cy, value);
  if (unit) { u8g2.setFont(F_SMALL); u8g2.drawStr(sx + vw + 3, cy, unit); }
}
void uiHint(const char* s) {
  u8g2.setFont(F_TINY);
  uiCenter(63, s);
}

// ---- list rows
int uiListRows() { return 3; }
void uiListRow(int slot, const char* label, const char* value, bool sel) {
  int top = BODY_Y + slot * 16;                 // 13, 29, 45
  int vw = (value && value[0]) ? u8g2.getStrWidth(value) : 0;
  if (sel) { u8g2.drawRBox(0, top, 125, 15, 2); u8g2.setDrawColor(0); }
  u8g2.setFont(F_UI);
  int labMax = 118 - vw - 4;
  uiTextClipped(5, top + 11, label, labMax);
  if (vw) u8g2.drawStr(122 - vw, top + 11, value);
  if (sel) u8g2.setDrawColor(1);
}
void uiScrollbar(int count, int topIndex, int rows) {
  if (count <= rows) return;
  int trackY = BODY_Y, trackH = SCR_H - BODY_Y;
  int thumbH = trackH * rows / count; if (thumbH < 6) thumbH = 6;
  int span = trackH - thumbH;
  int thumbY = trackY + (span * topIndex) / (count - rows);
  u8g2.drawBox(126, thumbY, 2, thumbH);
}

// ---- toast
static char     s_toast[24] = {0};
static uint32_t s_toastUntil = 0;
void toast(const char* msg) {
  strncpy(s_toast, msg, sizeof(s_toast) - 1);
  s_toast[sizeof(s_toast) - 1] = 0;
  s_toastUntil = millis() + 1300;
}
void toastUpdate() {
  if (millis() >= s_toastUntil || !s_toast[0]) return;
  u8g2.setFont(F_UI);
  int tw = u8g2.getStrWidth(s_toast);
  int bw = tw + 16; if (bw > 122) bw = 122;
  int bx = (SCR_W - bw) / 2, by = 40;
  u8g2.setDrawColor(1); u8g2.drawRBox(bx, by, bw, 18, 4);
  u8g2.setDrawColor(0); u8g2.drawRFrame(bx + 1, by + 1, bw - 2, 16, 3);
  char buf[24]; uiTruncate(buf, sizeof(buf), s_toast, bw - 12);
  u8g2.drawStr((SCR_W - u8g2.getStrWidth(buf)) / 2, by + 12, buf);
  u8g2.setDrawColor(1);
}

// ---- modal overlays
enum ModalType { MT_NONE, MT_CONFIRM, MT_INFO, MT_SLIDER, MT_CHOICE };
#define CHOICE_MAX 14
static char s_items[CHOICE_MAX][24]; static int s_nItems = 0, s_cSel = 0, s_cTop = 0;
static void (*s_onPick)(int) = nullptr;
static ModalType s_mt = MT_NONE;
static char s_title[22], s_msg[44], s_unit[8];
static void (*s_onYes)()   = nullptr;
static void (*s_onChange)(int) = nullptr;
static void (*s_onDone)()  = nullptr;
static int* s_val = nullptr;
static int  s_lo, s_hi, s_step;
static bool s_yesSel = false;

bool modalActive() { return s_mt != MT_NONE; }
void modalClose()  { s_mt = MT_NONE; s_onYes = s_onDone = nullptr; s_onChange = nullptr; s_val = nullptr; s_onPick = nullptr; }
void modalChoice(const char* title, const char* const* items, int n, int sel, void (*onPick)(int)) {
  s_mt = MT_CHOICE; s_onPick = onPick;
  s_nItems = min(n, CHOICE_MAX);
  for (int i = 0; i < s_nItems; i++) strlcpy(s_items[i], items[i], sizeof(s_items[0]));
  s_cSel = clampv(sel, 0, max(0, s_nItems - 1)); s_cTop = max(0, s_cSel - 3);
  strncpy(s_title, title, sizeof(s_title) - 1); s_title[sizeof(s_title)-1]=0;
  btnConsume(BTN_SELECT);
}

void modalConfirm(const char* title, const char* msg, void (*onYes)()) {
  s_mt = MT_CONFIRM; s_onYes = onYes; s_yesSel = false;
  strncpy(s_title, title, sizeof(s_title) - 1); s_title[sizeof(s_title)-1]=0;
  strncpy(s_msg, msg, sizeof(s_msg) - 1);       s_msg[sizeof(s_msg)-1]=0;
}
void modalInfo(const char* title, const char* msg) {
  s_mt = MT_INFO;
  strncpy(s_title, title, sizeof(s_title) - 1); s_title[sizeof(s_title)-1]=0;
  strncpy(s_msg, msg, sizeof(s_msg) - 1);       s_msg[sizeof(s_msg)-1]=0;
}
void modalSlider(const char* title, int* value, int lo, int hi, int step,
                 const char* unit, void (*onChange)(int), void (*onDone)()) {
  s_mt = MT_SLIDER; s_val = value; s_lo = lo; s_hi = hi; s_step = step;
  s_onChange = onChange; s_onDone = onDone;
  strncpy(s_title, title, sizeof(s_title) - 1); s_title[sizeof(s_title)-1]=0;
  strncpy(s_unit, unit ? unit : "", sizeof(s_unit) - 1); s_unit[sizeof(s_unit)-1]=0;
}

static void modalPanel() {
  u8g2.setDrawColor(1);
  u8g2.drawRBox(4, 6, 120, 14, 3);              // title bar
  u8g2.drawRFrame(4, 6, 120, 52, 3);            // outer
  u8g2.setDrawColor(0);
  u8g2.setFont(F_UI);
  uiCenter(16, s_title);
  u8g2.setDrawColor(1);
}

void modalUpdate() {
  if (s_mt == MT_CHOICE) {
    if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { modalClose(); fbBack(); return; }
    if (btnRepeat(BTN_DOWN)) { if (s_cSel < s_nItems - 1) { s_cSel++; fbNav(); } else fbEdge(); }
    if (btnRepeat(BTN_UP))   { if (s_cSel > 0) { s_cSel--; fbNav(); } else fbEdge(); }
    if (btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) {
      int pick = s_cSel; void (*cb)(int) = s_onPick;
      modalClose(); fbSelect(); if (cb) cb(pick); return;
    }
    const int rows = 4;
    if (s_cSel < s_cTop) s_cTop = s_cSel;
    if (s_cSel >= s_cTop + rows) s_cTop = s_cSel - rows + 1;
    u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 128, 64); u8g2.setDrawColor(1);
    u8g2.setFont(F_UI); uiTextClipped(2, 9, s_title, 124); u8g2.drawHLine(0, 11, 128);
    for (int r = 0; r < rows; r++) {
      int i = s_cTop + r; if (i >= s_nItems) break;
      int top = 13 + r * 12;
      if (i == s_cSel) { u8g2.drawRBox(0, top, 125, 12, 2); u8g2.setDrawColor(0); }
      u8g2.setFont(F_SMALL); uiTextClipped(4, top + 9, s_items[i], 118);
      u8g2.setDrawColor(1);
    }
    uiScrollbar(s_nItems, s_cTop, rows);
    return;
  }
  modalPanel();

  if (s_mt == MT_CONFIRM) {
    u8g2.setFont(F_SMALL);
    char buf[44]; uiTruncate(buf, sizeof(buf), s_msg, 108);
    uiCenter(34, buf);
    // buttons
    const int by = 42, bh = 12;
    for (int i = 0; i < 2; i++) {
      int bx = i == 0 ? 12 : 68;
      bool sel = (i == 1) == s_yesSel;
      if (sel) u8g2.drawRBox(bx, by, 48, bh, 2);
      if (sel) u8g2.setDrawColor(0);
      else     u8g2.drawRFrame(bx, by, 48, bh, 2);
      u8g2.setFont(F_SMALL);
      const char* t = i == 0 ? "Cancel" : "OK";
      u8g2.drawStr(bx + (48 - u8g2.getStrWidth(t)) / 2, by + 9, t);
      if (sel) u8g2.setDrawColor(1);
    }
    if (btnRepeat(BTN_LEFT))  { if (s_yesSel){ s_yesSel = false; fbNav(); } }
    if (btnRepeat(BTN_RIGHT)) { if (!s_yesSel){ s_yesSel = true;  fbNav(); } }
    if (btnPressed(BTN_SELECT)) {
      btnConsume(BTN_SELECT);
      bool yes = s_yesSel;
      void (*cb)() = s_onYes;
      modalClose();
      if (yes) { fbConfirm(); if (cb) cb(); }
      else fbBack();
    }
  }
  else if (s_mt == MT_INFO) {
    u8g2.setFont(F_SMALL);
    // allow a single \n split into two lines
    const char* nl = strchr(s_msg, '\n');
    if (nl) {
      char l1[32]; int n = nl - s_msg; if (n > 31) n = 31;
      memcpy(l1, s_msg, n); l1[n] = 0;
      uiCenter(30, l1);
      uiCenter(40, nl + 1);
    } else {
      char buf[44]; uiTruncate(buf, sizeof(buf), s_msg, 108);
      uiCenter(36, buf);
    }
    u8g2.drawRBox(40, 44, 48, 12, 2);
    u8g2.setDrawColor(0);
    u8g2.drawStr(40 + (48 - u8g2.getStrWidth("OK")) / 2, 53, "OK");
    u8g2.setDrawColor(1);
    if (btnPressed(BTN_SELECT)) { btnConsume(BTN_SELECT); modalClose(); fbBack(); }
  }
  else if (s_mt == MT_SLIDER && s_val) {
    int step = s_step;
    bool ch = false;
    if (btnRepeat(BTN_RIGHT)) { *s_val += step; ch = true; }
    if (btnRepeat(BTN_LEFT))  { *s_val -= step; ch = true; }
    if (btnRepeat(BTN_UP))    { *s_val += step * 5; ch = true; }
    if (btnRepeat(BTN_DOWN))  { *s_val -= step * 5; ch = true; }
    *s_val = clampv(*s_val, s_lo, s_hi);
    if (ch) { fbNav(); if (s_onChange) s_onChange(*s_val); }

    char v[16];
    snprintf(v, sizeof(v), "%d%s", *s_val, s_unit);
    u8g2.setFont(F_MEDB);
    uiCenter(38, v);
    float frac = (s_hi > s_lo) ? (float)(*s_val - s_lo) / (s_hi - s_lo) : 0;
    uiProgressBar(14, 44, 100, 8, frac);
    if (btnPressed(BTN_SELECT)) {
      btnConsume(BTN_SELECT);
      void (*cb)() = s_onDone; modalClose();
      fbSelect(); if (cb) cb();
    }
  }
}
