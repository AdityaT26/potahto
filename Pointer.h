// The gyro pointer shared by Air Mouse, Presenter and macros
//
// One motion engine (Settings > Air Mouse: axes, invert, sensitivity,
// acceleration, smoothing, screen size, tilt compensation) used everywhere a
// pointer is needed, so it feels identical in every app.
//
// Pointer hand-off: a macro can end with {pointer}, {pointer click} or
// {pointer drag} (e.g. Snip = {win+shift+s}{pointer drag}). Potahto then
// becomes an air mouse right where you are:
//   hold DOWN  move          SELECT  press / release the left button (drag)
//   RIGHT      right click   UP      re-center
//   LEFT       done          hold LEFT  Esc + done
// "click" / "drag" finish by themselves after one click / one drag.
#pragma once
#include <Arduino.h>

enum PointerMode : uint8_t { PTR_FREE = 0, PTR_CLICK, PTR_DRAG };

void pointerReset();                        // forget smoothing / remainders
void pointerRecenter();                     // capture the current roll as level
// Read the gyro and move the mouse. moving=false keeps the state fresh without
// moving (clutch released). scroll=true turns roll into the scroll wheel.
// Returns false when there is no motion sensor.
bool pointerStep(bool moving, bool scroll = false);
bool pointerClutch();                       // DOWN per Settings: true = should move now

void pointerStart(uint8_t mode, const char* title);   // macro hand-off overlay
bool pointerActive();
void pointerOverlayLoop();                  // main loop draws + handles input
void pointerCancel();
