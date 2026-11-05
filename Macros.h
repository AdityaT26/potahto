// Macro-pad profiles (stored in /macros.json, edited in web app)
//
// Two layouts per profile:
//   "hold"    : tap UP/DOWN/LEFT/RIGHT/SELECT = 5 macros, hold a direction =
//               4 more (9 per profile, the classic layout)
//   "folders" : tap = the same 5 macros, hold a direction = open that folder,
//               then tap one of its 5 macros (5 + 4 x 5 = 25 per profile)
// Key index:  0 UP 1 DOWN 2 LEFT 3 RIGHT 4 SELECT  (tap)
//             5..8  hold UP/DOWN/LEFT/RIGHT ("hold": a macro, "folders": the
//                   folder's name in the label)
//             9 + f*5 + i  folder f (0..3), item i (0 UP 1 DOWN 2 LEFT 3 RIGHT 4 SELECT)
// "app": ";"-separated PC programs (e.g. "Code.exe") - with the companion
// running, the pad switches to that profile when you switch apps.
// File: {"v":2,"profiles":[{"name":"VS Code","layout":"folders","app":"Code.exe",
//        "keys":[["Commands","{ctrl+shift+p}"], ...]}]}
#pragma once
#include <Arduino.h>

#define MACRO_KEYS      29
#define MACRO_TAP       5
#define MACRO_HOLD0     5
#define MACRO_FOLDER0   9
#define MACRO_PROFILES  16
#define MACRO_LABEL_MAX 11
#define MACRO_NAME_MAX  15
#define MACRO_APP_MAX   63

enum MacroLayout : uint8_t { ML_HOLD = 0, ML_FOLDERS };
struct MacroKey     { char label[MACRO_LABEL_MAX + 1]; String macro; };
struct MacroProfile {
  char name[MACRO_NAME_MAX + 1];
  uint8_t layout;
  char app[MACRO_APP_MAX + 1];
  MacroKey keys[MACRO_KEYS];
};

void        macrosBegin();                    // load names, register protocol commands
int         macroProfileCount();
const char* macroProfileName(int i);
bool        macroLoadProfile(int i, MacroProfile& out);
int         macroProfileForApp(const char* exe);   // -1 if none matches
const char* macroPcApp();                     // last app the companion reported ("" if none / stale)
uint32_t    macroPcAppAge();                  // ms since that report
