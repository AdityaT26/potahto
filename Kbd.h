// On-device text entry (WiFi passwords, names, macros) + pickers
//
// D-pad moves over a character grid, SELECT types. Bottom row: page switch
// (abc / ABC / 123 / #+=), space, delete, OK. Hold SELECT (let go) cancels.
// The text is only written back on OK.
#pragma once
#include <Arduino.h>

void kbdOpen(const char* title, char* buf, size_t cap, bool masked, void (*done)(bool ok));
bool kbdActive();
void kbdUpdate();           // main loop: draws + handles input while active

// Code entry for the lock screen: 4..6 presses of U/D/L/R, then SELECT.
void codeOpen(const char* title, void (*done)(const char* code));   // code = nullptr on cancel
bool codeActive();
void codeUpdate();
