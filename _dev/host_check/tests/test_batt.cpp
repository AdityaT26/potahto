#include "Multitool.h"
extern uint32_t g_fakeAdcMv;
int main() {
  g_nvs.clear(); settingsBegin(); S.battDivider = 2.0f; S.battTrim = 1.0f;
  // simulate a slow real discharge 4.10 V -> 3.70 V over 4000 readings, with +-10 mV noise
  int prev = -1, changes = 0, ups = 0;
  for (int i = 0; i <= 4000; i++) {
    float v = 4.10f - 0.40f * i / 4000.0f;
    g_fakeAdcMv = (uint32_t)(v / 2 * 1000) + ((i * 7) % 3 - 1) * 5;
    int p = battPercent();
    if (prev >= 0 && p != prev) { changes++; if (p > prev) ups++; }
    prev = p;
    if (i % 1000 == 0) printf("V=%.3f -> %d%%\n", v, p);
  }
  printf("changes=%d, upward blips during discharge=%d\n", changes, ups);
  return ups != 0;
}
