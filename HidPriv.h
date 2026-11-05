// Private: transport primitives (Hid.cpp) used by the macro engine (HidMacro.cpp).
#pragma once
#include <Arduino.h>
void    hidxKbd(uint8_t mods, uint8_t usage);   // full keyboard report (0,0 = release)
void    hidxCons(uint16_t usage);               // consumer report (0 = release)
uint8_t hidxButtons();                          // current mouse button state
bool    hidxIsUsb();                            // routed to USB (faster report pacing)
void    hidxPointer(uint8_t mode);              // {pointer ...}: start the pointer hand-off
void    hidxEvent(const char* kind, const char* arg);   // {pc ...}: event to the companion
void    hidMacroTick();                         // called by hidLoop()
