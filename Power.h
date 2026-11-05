// Screen off, lock screen, deep sleep (phone-style power)
//
// Screen off  : after Settings > Power > "Screen off after", or keep holding
//               SELECT past the back point (~1.5 s). The display, LED and idle
//               sensors go dark and the CPU drops to 80 MHz. Services keep
//               running: alarms, timer, pomodoro, Tripwire, Desk Guard, WiFi,
//               Bluetooth, Home Assistant. "Live" apps (Air Mouse, Presenter,
//               Macro Pad, Shutter, Smart Button) keep working blind: their
//               buttons still do their job and also light the screen.
//               Any other app pauses; the first press only wakes the screen.
// Lock screen : Off / Clock (any key) / Code (a 4-6 press D-pad code). It
//               appears when the screen was off longer than "Lock after".
// Deep sleep  : ~everything off, wake with UP, DOWN or LEFT (SELECT/RIGHT are
//               not wake-capable pins on the S3). Alarms wake it on time.
//               Automatic after "Deep sleep after" minutes of screen-off,
//               unless something needs to stay awake (armed guard, running
//               timer or pomodoro, USB host, WiFi set to Always on).
#pragma once
#include <Arduino.h>

enum PowerState : uint8_t { PWR_ON = 0, PWR_DIM, PWR_OFF };

void       powerBegin();               // very early in setup(): undo deep-sleep pin holds
bool       powerFastBoot();            // woke from deep sleep: skip the boot animation
void       powerLoop();                // every frame, right after inputUpdate()
PowerState powerState();
bool       powerScreenOn();            // ON or DIM
bool       powerLocked();              // the lock screen owns the display + input
void       powerScreenOff(bool lockNow);
void       powerWake();                // light the screen (e.g. alarm, notification)
void       powerPeek(uint32_t ms);     // light it briefly, then back off (notifications)
void       powerLockNow();
[[noreturn]] void powerDeepSleep();
void       powerRestart();
void       powerKeepAwake(uint8_t level);   // call each frame: 1 = may dim, 2 = stay fully on
void       powerDrawLock();            // main loop, when powerLocked()
void       powerDrawHoldHint();        // "release = back / keep holding = screen off"
bool       powerLedIdle();             // LED should be dark (inactivity / screen off)
const char* powerWakeReason();         // "button", "alarm", "power on", ...
