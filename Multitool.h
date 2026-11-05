// Umbrella header
//
// Every .cpp / .ino in this sketch includes only this file.  It pulls in the
// Arduino core, all third-party libraries and every module header in a known,
// fixed order, so include order is never a problem.
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

// ---- graphics
#include <U8g2lib.h>

// ---- visual feedback
#include <Adafruit_NeoPixel.h>

// ---- motion / environment sensors
#include "Config.h"
#if ENABLE_MPU
  #include <Adafruit_MPU6050.h>
  #include <Adafruit_Sensor.h>
#endif
#if ENABLE_TOF
  #include <VL53L0X.h>          // Pololu VL53L0X
#endif

// ---- our modules (order matters: types before the globals that use them)
#include "Settings.h"
#include "Feedback.h"
#include "Input.h"
#include "Sensors.h"
#include "UI.h"
#include "Apps.h"
#include "Games.h"
#include "Net.h"
#include "Hid.h"
#include "Store.h"
#include "Macros.h"
#include "Notify.h"
#include "Desk.h"
#include "Iot.h"
#include "Wireless.h"
#include "TimeSvc.h"
#include "Power.h"
#include "Watch.h"
#include "Music.h"
#include "Pointer.h"
#include "BleLink.h"
#include "Kbd.h"
#include "Globals.h"
