#pragma once
// OLED simulator: the real U8g2 library. The I2C byte driver is a no-op
// (oled_glue.cpp), so all drawing lands in the frame buffer like on the device.
#include_next <U8g2lib.h>
