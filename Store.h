// Small file store on LittleFS (the 1.5 MB "spiffs" partition)
//
// For data that is too big or too structured for NVS settings: macro-pad
// profiles, smart-home config, game saves. Firmware uploads (USB or OTA) only
// rewrite the app partition, so these files survive updates just like
// settings do. Writes are atomic (temp file + rename).
// Needs a partition scheme with a data partition, e.g. the XIAO default
// "Default with spiffs (3MB APP/1.5MB SPIFFS)". Without one, storeBegin()
// returns false and features fall back to their built-in defaults.
#pragma once
#include <Arduino.h>

bool   storeBegin();
bool   storeOk();
bool   storeRead(const char* path, String& out, size_t maxLen = 32768);
bool   storeWrite(const char* path, const char* data, size_t len);
bool   storeRemove(const char* path);
size_t storeFreeBytes();
