// ToF background services: Tripwire and Desk Guard
//
// Both keep running after you leave their app, with the screen off and the
// device locked, until you disarm them from the app (or the web app).
// Tripwire   : alarm when something comes closer than the trip distance.
// Desk Guard : "Guard my seat"   - runs a macro when you leave (default Win+L)
//                                  and another when you come back
//              "Watch behind me" - point the sensor at the door / behind you;
//                                  runs a macro when someone comes close
//                                  (default Alt+Tab, or Win+D, or {run ...})
// Macros go out over USB or Bluetooth like the Macro Pad's.
#pragma once
#include <Arduino.h>

void watchBegin();               // protocol commands
void watchLoop();                // every frame
bool watchBusy();                // anything armed
const char* watchStatusLine();   // for the lock screen

// tripwire
void     tripArm(bool on);
bool     tripArmed();
bool     tripTripped();
void     tripSilence();          // stop the siren, stay armed

// desk guard
enum GuardState : uint8_t { GS_OFF = 0, GS_PRESENT, GS_LEAVING, GS_AWAY, GS_WATCHING, GS_INTRUDER };
void       guardArm(bool on);
bool       guardArmed();
GuardState guardState();
uint32_t   guardLeavingMs();     // time left before "away"
const char* guardLastAction();   // what it last did ("Win+L sent", ...)

// shared distance (mm, 0 = none) for the UIs
uint16_t watchDistanceMm();
bool     watchDistanceOk();
