#pragma once
#include <Arduino.h>
extern uint16_t g_fakeTofMm;     // set by the simulators
class VL53L0X {
 public:
  enum regAddr { SYSTEM_INTERRUPT_CLEAR = 0x0B, RESULT_INTERRUPT_STATUS = 0x13, RESULT_RANGE_STATUS = 0x14 };
  void setTimeout(uint16_t) {}
  bool init(bool = true) { return true; }
  void startContinuous(uint32_t = 0) {}
  void stopContinuous() {}
  uint16_t readRangeContinuousMillimeters() { return g_fakeTofMm; }
  uint16_t readRangeSingleMillimeters() { return g_fakeTofMm; }
  uint8_t readReg(uint8_t) { return 0x07; }
  uint16_t readReg16Bit(uint8_t) { return g_fakeTofMm; }
  void writeReg(uint8_t, uint8_t) {}
  bool timeoutOccurred() { return false; }
  bool setMeasurementTimingBudget(uint32_t) { return true; }
};
