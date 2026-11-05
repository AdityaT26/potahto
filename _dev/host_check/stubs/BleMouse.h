#pragma once
#include <Arduino.h>
#define MOUSE_LEFT 1
#define MOUSE_RIGHT 2
#define MOUSE_MIDDLE 4
class BleMouse {
 public:
  BleMouse(const char* = "", const char* = "", uint8_t = 100) {}
  void begin() {} void end() {}
  bool isConnected() { return false; }
  void move(signed char, signed char, signed char = 0, signed char = 0) {}
  void click(uint8_t = MOUSE_LEFT) {}
  void press(uint8_t = MOUSE_LEFT) {}
  void release(uint8_t = MOUSE_LEFT) {}
  void setBatteryLevel(uint8_t) {}
};
