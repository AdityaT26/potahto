#pragma once
#include <Arduino.h>
struct IPAddress { uint8_t b[4]; String toString() const { char s[16]; snprintf(s, 16, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]); return String(s); } };
class WiFiClass {
 public:
  String macAddress() { return String("AA:BB:CC:DD:EE:FF"); }
  void macAddress(uint8_t* m) { uint8_t x[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}; memcpy(m, x, 6); }
};
extern WiFiClass WiFi;
