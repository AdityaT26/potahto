// Single definition point for all shared objects
#include "Multitool.h"

// Persistent settings (declared in Settings.h)
Settings S;

// Display: SSD1306 128x64, hardware I2C, full frame buffer
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Single WS2812 pixel
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// NVS handle
Preferences prefs;

#if ENABLE_MPU
  Adafruit_MPU6050 mpu;
#endif
#if ENABLE_TOF
  VL53L0X tof;
#endif

bool g_bleConnected = false;
bool g_customFrame  = false;
