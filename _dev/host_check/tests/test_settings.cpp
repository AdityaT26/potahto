#include "Multitool.h"
#include "SettingsLegacy.h"
#include <stddef.h>
#include <assert.h>
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
static void reboot() { prefs.end(); settingsBegin(); }

int main() {
  // 1) fresh device -> defaults, requested air-mouse values
  g_nvs.clear(); settingsBegin();
  CHECK(S.mouseAxisH == 2 && S.mouseAxisV == 1 && S.mouseAxisS == 0);
  CHECK(S.mouseInvertX && !S.mouseInvertY && !S.mouseTiltComp && S.mouseHoldMoves);
  CHECK(S.scrollSens == 3 && S.gAimAxisH == 2 && S.gAimAxisV == 1);
  CHECK(prefs.getUShort("schema", 0) == SETTINGS_VERSION);

  // 2) settings survive a "firmware update" (reboot with same NVS)
  S.buzzerVolume = 2; S.battDivider = 2.37f; S.gyroBias[1] = -1.25f; S.tripDistMm = 777;
  S.magMin[2] = -444; S.scaleTareMm = -12; settingsSave();
  reboot();
  CHECK(S.buzzerVolume == 2 && fabsf(S.battDivider - 2.37f) < 1e-6 && S.gyroBias[1] == -1.25f);
  CHECK(S.tripDistMm == 777 && S.magMin[2] == -444 && S.scaleTareMm == -12);

  // 3) a NEW field added in a later firmware takes its default: simulate by
  //    deleting a key (= key never existed on the old firmware)
  prefs.remove("trVol"); reboot();
  CHECK(S.tripVolume == 8 && S.buzzerVolume == 2);

  // 4) save skips unchanged keys (flash wear)
  int w0 = g_nvsWrites; settingsSave(); CHECK(g_nvsWrites == w0);
  S.lightHue = 99; w0 = g_nvsWrites; settingsSave(); CHECK(g_nvsWrites == w0 + 1);

  // 5) factory reset keeps hi-scores / IR slots
  prefs.putUInt("hsSnake", 42); settingsFactoryReset(); reboot();
  CHECK(S.buzzerVolume == 7 && prefs.getUInt("hsSnake", 0) == 42);

  // 6) legacy v6 blob migration: calibration kept, mouse orientation forced
  g_nvs.clear(); prefs.begin("mtl");
  settingsLoadDefaults();
  SettingsV6 old; memcpy(&old, &S, offsetof(SettingsV6, gAimInvV) + 1);
  old.version = 6; old.battDivider = 2.11f; old.gyroBias[0] = 3.5f;
  old.buzzerVolume = 4; old.mouseAxisH = 0; old.mouseInvertX = false; old.scrollSens = 9;
  old.gAimAxisV = 0; old.compassCalibrated = true; old.magMax[0] = 1234;
  prefs.putBytes("cfg", &old, sizeof(old));
  reboot();
  CHECK(fabsf(S.battDivider - 2.11f) < 1e-6 && S.gyroBias[0] == 3.5f && S.buzzerVolume == 4);
  CHECK(S.compassCalibrated && S.magMax[0] == 1234);
  CHECK(S.mouseAxisH == 2 && S.mouseInvertX && S.scrollSens == 3 && S.gAimAxisV == 1);
  CHECK(!prefs.isKey("cfg"));
  reboot();                                       // and it persists afterwards
  CHECK(fabsf(S.battDivider - 2.11f) < 1e-6 && S.mouseAxisH == 2);

  // 7) corrupt/foreign legacy blob is ignored safely
  g_nvs.clear(); prefs.begin("mtl"); uint8_t junk[10] = {1,2,3};
  prefs.putBytes("cfg", junk, 10); reboot();
  CHECK(S.buzzerVolume == 7 && !prefs.isKey("cfg"));

  // 8) type change of a key (e.g. u8 -> u16 in a future fw) re-defaults
  g_nvs.clear(); settingsBegin(); prefs.putUInt("trDist", 5); reboot();
  CHECK(S.tripDistMm == 300);

  // 9) every key fits NVS's 15-char limit and keys are unique
  size_t n; const SettingField* F = settingsFields(&n);
  for (size_t i = 0; i < n; i++) { CHECK(strlen(F[i].key) <= 15);
    for (size_t j = i + 1; j < n; j++) CHECK(strcmp(F[i].key, F[j].key) != 0); }
  // 10) strings persist; PIN auto-generated, 6 digits, stable across boots
  g_nvs.clear(); settingsBegin();
  CHECK(strlen(S.webPin) == 6 && !strcmp(S.devName, "Potahto") && !strcmp(S.tz, "IST-5:30"));
  char pin[8]; strcpy(pin, S.webPin);
  strcpy(S.wifiSsid[1], "HomeNet"); strcpy(S.wifiPass[1], "hunter22"); settingsSave(); reboot();
  CHECK(!strcmp(S.wifiSsid[1], "HomeNet") && !strcmp(S.wifiPass[1], "hunter22") && !strcmp(S.webPin, pin));
  settingsFactoryReset(); reboot();
  CHECK(S.wifiSsid[1][0] == 0 && !strcmp(S.webPin, pin));

  // 11) typed setters clamp / validate
  const SettingField* f = settingsFind("mSens");
  CHECK(settingsSetFromDouble(*f, 99) && S.mouseSens == 20);
  CHECK(settingsSetFromString(*f, "5") && S.mouseSens == 5);
  CHECK(!settingsSetFromString(*f, "abc") && S.mouseSens == 5);
  f = settingsFind("mScreen"); CHECK(settingsSetFromDouble(*f, 7) && S.mouseScreen == 3);
  f = settingsFind("battDiv"); CHECK(settingsSetFromDouble(*f, 2.345) && fabsf(S.battDivider - 2.345f) < 1e-5);
  f = settingsFind("devName"); CHECK(!settingsSetFromString(*f, "this name is far too long for the field"));
  CHECK(!settingsSetFromString(*f, "bad\nname") && settingsSetFromString(*f, "Spud") && !strcmp(S.devName, "Spud"));
  f = settingsFind("mInvX"); CHECK(settingsSetFromString(*f, "false") && !S.mouseInvertX);
  CHECK(settingsFind("nope") == nullptr);
  f = settingsFind("gyroBias"); CHECK(!settingsSetFromDouble(*f, 1));

  printf("settings: all %zu fields, 11 scenarios PASS\n", n);

  // 10) battery: no flicker on +-10 mV noise around a boundary
  extern uint32_t g_fakeAdcMv;
  S.battDivider = 2.0f; S.battTrim = 1.0f;
  int changes = 0, last = -1;
  for (int i = 0; i < 2000; i++) {
    g_fakeAdcMv = 2034 + ((i % 3) - 1) * 5;          // ~4.068 V +-10 mV
    int p = battPercent(); if (last >= 0 && p != last) changes++; last = p;
  }
  printf("battery: settled at %d%%, %d changes over 2000 noisy samples\n", last, changes);
  CHECK(changes <= 3);
  return 0;
}
