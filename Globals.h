// Shared hardware objects (declared here, defined in Globals.cpp)
#pragma once
// NOTE: this header is included last by Multitool.h, so every library type
// below is already known.

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_NeoPixel strip;
extern Preferences prefs;

#if ENABLE_MPU
  extern Adafruit_MPU6050 mpu;
#endif
#if ENABLE_TOF
  extern VL53L0X tof;
#endif
// IR uses the native RMT peripheral directly (no library objects needed).

// cross-app runtime flags
extern bool g_bleConnected;    // a Bluetooth HID host is connected (status bar "B")
extern bool g_customFrame;     // games own the frame: main loop skips clear/send
