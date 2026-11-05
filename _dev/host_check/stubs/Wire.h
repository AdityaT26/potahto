#pragma once
#include <Arduino.h>
class TwoWire {
 public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
  void setClock(uint32_t) {}
  uint8_t addr = 0;
  void beginTransmission(uint8_t a) { addr = a; }
  uint8_t endTransmission(bool = true) { return (addr == 0x29 || addr == 0x68) ? 0 : 2; }   // ToF + MPU "present"
  size_t write(uint8_t) { return 1; }
  uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
  int available() { return 0; }
  int read() { return 0; }
};
extern TwoWire Wire;
