// Central hardware + build configuration
//
// XIAO ESP32-S3 Multitool
// Everything that is board- or taste-specific lives here so the rest of the
// firmware never hard-codes a pin number or magic constant.
#pragma once
#include <Arduino.h>

// ---- Firmware identity
#define FW_NAME     "POTAHTO"
#define FW_VERSION  "1.3.0"

// ---- Pin map  (GPIO numbers, not the silk-screen D-numbers)
#define PIN_BTN_UP      4
#define PIN_BTN_DOWN    2
#define PIN_BTN_LEFT    3
#define PIN_BTN_RIGHT   43
#define PIN_BTN_SELECT  44

#define PIN_MOTOR       7     // vibration motor  (haptics)
#define PIN_BUZZER      16    // passive buzzer   (audio)
#define PIN_NEOPIXEL    12    // WS2812           (visual)
#define NEOPIXEL_COUNT  1     // single RGB LED

#define PIN_IR_TX       9     // IR transmitter
#define PIN_IR_RX       8     // IR receiver (demodulated)

#define PIN_BATTERY     1     // ADC1_CH0, through voltage divider

#define PIN_I2C_SDA     5     // D4
#define PIN_I2C_SCL     6     // D5
#define I2C_CLOCK_HZ    400000UL

// I2C device addresses
#define ADDR_OLED       0x3C
#define ADDR_MPU6050    0x68
#define ADDR_HMC5883L   0x1E   // Honeywell
#define ADDR_QMC5883L   0x0D   // cheap GY-271 clone
#define ADDR_VL53L0X    0x29

// ---- Button electrical config
// Buttons wired to GND, using the internal pull-up -> pressed reads LOW.
#define BTN_ACTIVE_LOW  1

// ---- Vibration motor drive polarity
// This build uses a PNP high-side driver (e.g. S8550): base LOW = motor ON.
// So the pin is active-LOW and idle must be HIGH, or the motor runs forever.
// Set to 0 if you ever switch to a low-side NPN/MOSFET driver.
#define MOTOR_ACTIVE_LOW  1

// ---- Feature build switches  (turn a whole subsystem off at compile time)
#define ENABLE_IR         1
#define ENABLE_COMPASS    1
#define ENABLE_TOF        1
#define ENABLE_MPU        1

// ---- Display geometry
#define SCR_W   128
#define SCR_H   64
#define HEADER_H 13          // top status bar height (rows 0..12)
#define BODY_Y   HEADER_H    // first usable body row

// ---- Battery measurement
// Vbat = Vadc * BATT_DIVIDER.  A plain 100k/100k divider = 2.0.
// These are safe defaults; the value is stored in NVS and tunable in Settings.
#define BATT_DIVIDER_DEFAULT   2.00f
#define BATT_ADC_ATTEN         ADC_11db     // full 0..~3.1V range
#define BATT_FULL_V            4.20f
#define BATT_EMPTY_V           3.30f

// ---- Timing (ms)
#define FRAME_MS            33      // ~30 fps UI target
#define BTN_DEBOUNCE_MS     18
#define BTN_LONGPRESS_MS    550
#define BTN_POWER_MS        1500    // keep holding SELECT: screen off
#define BTN_REPEAT_DELAY_MS 420     // wait before auto-repeat kicks in
#define BTN_REPEAT_RATE_MS  110     // auto-repeat interval
#define IDLE_DIM_MS         30000UL // dim/idle neopixel after inactivity

// ---- Feedback intensity ceilings (0..255)
#define MOTOR_PWM_MAX      255
#define NEOPIXEL_PWM_MAX   255

// ---- ESP32 Arduino-core LEDC compatibility shim
// Core 3.x : ledcAttach(pin,freq,res) / ledcWrite(pin,duty) / ledcWriteTone(pin,freq)
// Core 2.x : channel based API
#if !defined(ESP_ARDUINO_VERSION_MAJOR)
  #include "esp_arduino_version.h"
#endif
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #define LEDC_CORE3 1
#else
  #define LEDC_CORE3 0
#endif

// LEDC channels only used on core 2.x
#define LEDC_CH_MOTOR   2
#define LEDC_CH_BUZZER  3
#define LEDC_MOTOR_FREQ 20000     // above hearing, smooth motor PWM
#define LEDC_MOTOR_RES  8         // 8-bit duty

// ---- Small helpers
#define ARRAY_LEN(a)  (sizeof(a) / sizeof((a)[0]))
template <typename T> static inline T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
