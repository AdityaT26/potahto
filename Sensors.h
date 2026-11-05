// MPU6050, compass (HMC5883L / QMC5883L), VL53L0X ToF, battery
#pragma once
#include <Arduino.h>

struct Vec3f { float x, y, z; };

void  sensorsBegin();          // scans I2C and initialises whatever is present

bool  mpuPresent();
bool  compassPresent();
bool  tofPresent();
const char* compassChipName(); // "HMC5883L" / "QMC5883L" / "-"

// ---- MPU6050
bool  mpuRead(Vec3f& accel_ms2, Vec3f& gyro_dps, float& tempC); // gyro bias applied
void  accelAngles(const Vec3f& a, float& pitchDeg, float& rollDeg);
void  mpuCalibrateGyro();      // ~1 s, device must be still -> S.gyroBias (saved)
void  mpuLevelHere();          // capture current tilt as "level" (saved)

// ---- Battery
float   battVoltage();         // volts (smoothed)
uint8_t battPercent();         // 0..100 (LiPo curve)
bool    battCharging();        // best-effort: voltage rising above full

// ---- Compass
bool  compassReadRaw(Vec3f& raw);
float compassHeading();        // degrees 0..360 (cal + declination applied)
void  compassCalReset();       // begin a min/max capture
void  compassCalSample();      // fold one reading into min/max
void  compassCalFinish();      // store & mark calibrated (saved)
void  compassSetNorth();       // point at true north, then call: sets magOffset

// ---- ToF (VL53L0X)
bool     tofReadMm(uint16_t& mm);   // latest sample (non-blocking); false if none / out of range
uint16_t tofReadAvg(uint8_t samples); // waits for fresh samples, 0 on failure

// ---- power
void     sensorsIdleTick();         // every frame: sleeps sensors nobody read for a while
void     sensorsSleepAll();         // before deep sleep
