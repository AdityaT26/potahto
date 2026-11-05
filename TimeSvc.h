// One clock for everything (status bar, alarms, lock screen)
//
// Sources, best first: WiFi (NTP), the web app / companion over USB, WiFi or
// Bluetooth ("time.set"), a phone's Current Time Service over Bluetooth, or
// set by hand in Settings > Time. The ESP32 keeps the time across screen-off,
// deep sleep and restarts; only pulling the battery loses it.
#pragma once
#include <Arduino.h>
#include <time.h>

void        timeBegin();                         // apply the time zone (setup)
bool        timeValid();                         // the clock has been set
bool        timeNow(struct tm& lt);              // local time; false if unknown
void        timeSetEpoch(double epoch, const char* source);   // ignores tiny drifts
void        timeSetLocal(int y, int mo, int d, int h, int mi); // manual (local time)
void        timeSetLocalS(int y, int mo, int d, int h, int mi, int sec, const char* source);
const char* timeSource();                        // "WiFi", "USB", "Bluetooth", "Phone", "Manual", "kept"
uint32_t    timeSetAgoS();                       // seconds since the last sync (0 = never this boot)
void        timeFmtHM(char* out, size_t n, const struct tm& lt);   // "14:05" or "2:05"
const char* timeAmPm(const struct tm& lt);       // "AM"/"PM" in 12-h mode, "" in 24-h
void        timeApplyTz();                       // after S.tz changes
