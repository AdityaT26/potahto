// Boot animation, jingle and sensor self-test
#include "Multitool.h"

void startupSequence() {
  npxSetBase(NPX_RAINBOW, 0, 8);
  fbBoot();                                   // gated internally by settings

  if (!S.startupAnim) {                       // quiet boot
    for (int i = 0; i < 6; i++) { fbUpdate(); delay(FRAME_MS); }
    npxFlash(strip.Color(0, 180, 40), 1, 150);
    return;
  }

  struct Item { const char* name; bool ok; };
  Item items[4] = {
    { "Display", true },
    { "Motion",  mpuPresent() },
    { "Compass", compassPresent() },
    { "Range",   tofPresent() },
  };

  const uint32_t DUR = 1900;
  uint32_t t0 = millis();
  for (;;) {
    inputUpdate();
    fbUpdate();
    uint32_t el = millis() - t0;

    uiFrameBegin();
    uiGlyph(GLYPH_TOOL, 6, 5, 26);

    // wordmark wipe-in
    u8g2.setFont(F_MEDB);
    u8g2.drawStr(38, 22, FW_NAME);
    int reveal = map((int)min(el, (uint32_t)450), 0, 450, 0, 92);
    u8g2.setDrawColor(0);
    u8g2.drawBox(38 + reveal, 4, 92 - reveal, 22);
    u8g2.setDrawColor(1);
    u8g2.setFont(F_TINY);
    if (el > 250) u8g2.drawStr(38, 30, "v" FW_VERSION);

    // self-test list revealed one row at a time
    if (el > 480) {
      u8g2.setFont(F_SMALL);
      int shown = (int)((el - 480) / 170); if (shown > 4) shown = 4;
      for (int i = 0; i < shown; i++) {
        int y = 42 + i * 6;                    // 42,48,54,60  (all on-screen)
        u8g2.drawStr(8, y, items[i].name);
        const char* st = items[i].ok ? "OK" : "--";
        u8g2.drawStr(112, y, st);
        if (items[i].ok) { u8g2.drawLine(104, y - 3, 106, y); u8g2.drawLine(106, y, 110, y - 5); }
      }
    }
    uiFrameEnd();

    if (el >= DUR) break;
    if (btnPressed(BTN_SELECT)) break;          // skip
    delay(FRAME_MS);
  }
  npxFlash(strip.Color(0, 180, 40), 2, 140);
}
