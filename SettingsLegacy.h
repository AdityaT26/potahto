// frozen copy of the v1.0 (schema 6) Settings layout
//
// Firmware <= 1.0 stored the whole struct below as one NVS blob ("cfg").
// It is used only to migrate that blob once (Settings.cpp). Never edit it.
// The live Settings struct starts with exactly these members in this order,
// so the common prefix can be copied directly (static_assert-checked).
#pragma once
#include <Arduino.h>

struct SettingsV6 {
  uint32_t magic;
  uint16_t version;

  // ---- feedback master switches + strengths
  bool     buzzerEnabled;
  bool     motorEnabled;
  bool     neopixelEnabled;
  uint8_t  buzzerVolume;      // 0..10
  uint8_t  motorStrength;     // 0..10
  uint8_t  neopixelBright;    // 0..255  (global brightness cap)
  uint8_t  motorDurPct;       // 20..200 % scale of motor pulse durations
  bool     motorDurAll;       // also scale the long/special pulses

  // ---- startup
  bool     startupAnim;
  bool     startupSound;

  // ---- display
  bool     rotate180;
  bool     autoRotate;
  uint8_t  autoRotateThresh;  // tilt degrees needed to flip (10..80)

  // ---- battery
  float    battDivider;
  float    battTrim;          // fine multiplicative trim (~1.0)

  // ---- MPU6050 calibration
  bool     mpuCalibrated;
  float    gyroBias[3];       // deg/s
  float    accelBias[3];      // m/s^2, levels the bubble

  // ---- compass calibration
  bool     compassCalibrated;
  int16_t  magMin[3];
  int16_t  magMax[3];
  float    magDeclination;    // degrees, +E
  float    magOffset;         // manual heading offset (aligns sensor to screen)

  // ---- air-mouse
  uint8_t  mouseSens;         // 1..20
  uint8_t  scrollSens;        // 1..10
  bool     mouseInvertX;
  bool     mouseInvertY;
  uint8_t  mouseAxisH;        // gyro axis driving horizontal: 0=X 1=Y 2=Z
  uint8_t  mouseAxisV;        // gyro axis driving vertical:   0=X 1=Y 2=Z
  uint8_t  mouseAxisS;        // gyro axis driving scroll wheel: 0=X 1=Y 2=Z
  bool     mouseTiltComp;     // rotate motion by held-roll (horizon lock)
  uint8_t  mouseAccel;        // 0..10 pointer acceleration (nonlinear gain)
  uint8_t  mouseSmooth;       // 0..10 motion smoothing (low-pass)
  uint8_t  mouseScreen;       // 0=720p 1=1080p 2=1440p 3=4K (scales gain)
  bool     mouseHoldMoves;    // DOWN: false=hold-to-freeze, true=hold-to-move

  // ---- tripwire
  uint16_t tripDistMm;        // trigger closer than this
  bool     tripUseMotor;
  bool     tripUseBuzzer;
  bool     tripUseLed;
  uint8_t  tripVolume;        // 0..10 (local buzzer volume)
  bool     tripAutoReset;     // true: clears when object leaves; false: latches

  // ---- light tool
  uint8_t  lightHue;          // 0..255
  uint8_t  lightBright;       // 0..255
  uint8_t  lightMode;         // LightMode

  // ---- scale
  uint8_t  scaleUnit;         // ScaleUnit
  int16_t  scaleTareMm;

  // ---- games
  bool     gameMusic;         // chiptune background track (during games only)
  bool     gameSfx;           // game sound effects
  bool     gameHaptics;       // game vibration
  bool     gameNeopixel;      // game LED effects
  // motion mapping for sensor games (which axis drives which game motion)
  uint8_t  gTiltAxisH; bool gTiltInvH;   // accel -> horizontal
  uint8_t  gTiltAxisV; bool gTiltInvV;   // accel -> vertical
  uint8_t  gAimAxisH;  bool gAimInvH;    // gyro  -> horizontal
  uint8_t  gAimAxisV;  bool gAimInvV;    // gyro  -> vertical
};
