// Voltage / percentage / charge state
#include "Multitool.h"

void batteryEnter() { /* neopixel set live in loop */ }

void batteryLoop() {
  if (wantBack() || btnTap(BTN_LEFT)) { fbBack(); goBack(); return; }

  float v = battVoltage();
  int   pct = battPercent();
  bool  chg = battCharging();

  // live neopixel: green>50, amber>20, red otherwise, breathing while charging
  uint32_t c = pct > 50 ? strip.Color(0, 170, 30)
             : pct > 20 ? strip.Color(190, 120, 0)
                        : strip.Color(200, 0, 0);
  npxSetBase(chg ? NPX_BREATHE : NPX_SOLID, c, 3);

  uiHeader("Battery");

  // big battery body
  const int bx = 12, by = 20, bw = 96, bh = 26;
  u8g2.drawRFrame(bx, by, bw, bh, 3);
  u8g2.drawBox(bx + bw, by + 8, 4, 10);                       // terminal
  int iw = (int)((bw - 8) * (pct / 100.0f));
  if (iw > 0) u8g2.drawBox(bx + 4, by + 4, iw, bh - 8);        // fill

  // percentage over the fill, XOR so it reads on both halves
  char ps[8]; snprintf(ps, sizeof(ps), "%d%%", pct);
  u8g2.setFont(F_MEDB);
  u8g2.setDrawColor(2);
  u8g2.drawStr(bx + (bw - u8g2.getStrWidth(ps)) / 2, by + 18, ps);
  u8g2.setDrawColor(1);

  // voltage + state line
  char line[24];
  snprintf(line, sizeof(line), "%.2f V", v);
  u8g2.setFont(F_UI);
  u8g2.drawStr(6, 60, line);
  const char* st = chg ? "charging" : (pct <= 15 ? "low!" : "");
  if (st[0]) uiRight(122, 60, st);
}
