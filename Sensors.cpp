// MPU6050 / compass / VL53L0X / battery
#include "Multitool.h"
#include <math.h>

static bool s_mpuOk = false, s_compassOk = false, s_tofOk = false;
enum CompassType { CT_NONE, CT_HMC, CT_QMC };
static CompassType s_ct = CT_NONE;
// idle power management: sensors sleep a few seconds after their last use
static uint32_t s_mpuUse = 0, s_magUse = 0, s_tofUse = 0;
static bool     s_mpuAwake = true, s_magAwake = true, s_tofRun = false;
static uint16_t s_tofMm = 0; static bool s_tofValid = false; static uint32_t s_tofAt = 0;

// ---- low level I2C
static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
static void wReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}
static bool rBytes(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t got = Wire.requestFrom(addr, n);
  uint8_t i = 0;
  while (i < n && Wire.available()) buf[i++] = Wire.read();
  return got == n;
}

// ---- compass init
static void initQMC() {
  wReg(ADDR_QMC5883L, 0x0B, 0x01);   // SET/RESET period
  wReg(ADDR_QMC5883L, 0x09, 0x1D);   // OSR=512, RNG=8G, ODR=200Hz, continuous
}
static void initHMC() {
  wReg(ADDR_HMC5883L, 0x00, 0x70);   // 8-avg, 15Hz, normal
  wReg(ADDR_HMC5883L, 0x01, 0xA0);   // gain
  wReg(ADDR_HMC5883L, 0x02, 0x00);   // continuous
}

// ---- lifecycle
void sensorsBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  analogReadResolution(12);
#if defined(ADC_11db)
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
#endif

#if ENABLE_MPU
  if (i2cPresent(ADDR_MPU6050) && mpu.begin(ADDR_MPU6050, &Wire)) {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    s_mpuOk = true;
  }
#endif

#if ENABLE_COMPASS
  if (i2cPresent(ADDR_QMC5883L))      { s_ct = CT_QMC; initQMC(); s_compassOk = true; }
  else if (i2cPresent(ADDR_HMC5883L)) { s_ct = CT_HMC; initHMC(); s_compassOk = true; }
#endif

#if ENABLE_TOF
  if (i2cPresent(ADDR_VL53L0X)) {
    tof.setTimeout(500);
    if (tof.init()) { s_tofOk = true; s_tofRun = false; }     // started on first use
  }
#endif
}

bool mpuPresent()     { return s_mpuOk; }
bool compassPresent() { return s_compassOk; }
bool tofPresent()     { return s_tofOk; }
const char* compassChipName() {
  return s_ct == CT_QMC ? "QMC5883L" : (s_ct == CT_HMC ? "HMC5883L" : "-");
}

// ---- MPU6050
bool mpuRead(Vec3f& accel, Vec3f& gyro, float& tempC) {
#if ENABLE_MPU
  if (!s_mpuOk) return false;
  s_mpuUse = millis();
  if (!s_mpuAwake) { mpu.enableSleep(false); s_mpuAwake = true; delay(2); }
  sensors_event_t a, g, t;
  if (!mpu.getEvent(&a, &g, &t)) return false;
  accel = { a.acceleration.x, a.acceleration.y, a.acceleration.z };
  gyro  = { g.gyro.x * 57.29578f - S.gyroBias[0],
            g.gyro.y * 57.29578f - S.gyroBias[1],
            g.gyro.z * 57.29578f - S.gyroBias[2] };
  tempC = t.temperature;
  return true;
#else
  return false;
#endif
}
void accelAngles(const Vec3f& a, float& pitchDeg, float& rollDeg) {
  pitchDeg = atan2f(-a.x, sqrtf(a.y * a.y + a.z * a.z)) * 57.29578f;
  rollDeg  = atan2f(a.y, a.z) * 57.29578f;
}
void mpuCalibrateGyro() {
#if ENABLE_MPU
  if (!s_mpuOk) return;
  double sx = 0, sy = 0, sz = 0; int n = 0;
  uint32_t end = millis() + 1000;
  while (millis() < end) {
    sensors_event_t a, g, t;
    if (mpu.getEvent(&a, &g, &t)) {
      sx += g.gyro.x; sy += g.gyro.y; sz += g.gyro.z; n++;
    }
    delay(3);
  }
  if (n > 0) {
    S.gyroBias[0] = (sx / n) * 57.29578f;
    S.gyroBias[1] = (sy / n) * 57.29578f;
    S.gyroBias[2] = (sz / n) * 57.29578f;
    S.mpuCalibrated = true;
    settingsSave();
  }
#endif
}
void mpuLevelHere() {
#if ENABLE_MPU
  if (!s_mpuOk) return;
  Vec3f a, g; float t;
  if (mpuRead(a, g, t)) {
    float p, r; accelAngles(a, p, r);
    S.accelBias[0] = p;    // pitch offset
    S.accelBias[1] = r;    // roll offset
    settingsSave();
  }
#endif
}

// ---- battery
float battVoltage() {
  static float ema = -1;
  uint32_t mv = analogReadMilliVolts(PIN_BATTERY);
  float v = (mv / 1000.0f) * S.battDivider * S.battTrim;
  // Heavier low-pass than before (0.2 -> 0.05): ADC noise of a few mV no longer
  // walks the voltage up and down each reading. Settles in ~1-2 s, which is
  // plenty fast for a battery gauge.
  if (ema < 0) ema = v; else ema += (v - ema) * 0.05f;
  return ema;
}
// Raw LiPo-curve percentage from the (already smoothed) voltage.
static uint8_t battPercentRaw() {
  static const float vt[] = {3.30f,3.40f,3.50f,3.60f,3.70f,3.80f,3.90f,4.00f,4.10f,4.20f};
  static const float pt[] = {0,    5,    10,   20,   35,   50,   65,   80,   90,   100};
  float v = battVoltage();
  if (v <= vt[0]) return 0;
  if (v >= vt[9]) return 100;
  for (int i = 0; i < 9; i++) {
    if (v < vt[i + 1]) {
      float f = (v - vt[i]) / (vt[i + 1] - vt[i]);
      return (uint8_t)(pt[i] + f * (pt[i + 1] - pt[i]) + 0.5f);
    }
  }
  return 100;
}
uint8_t battPercent() {
  // Hysteresis / deadband on top of the smoothed voltage: the shown number only
  // moves when the raw value has drifted at least 2 points away from it, so a
  // battery sitting on a boundary stops flickering 87<->88. 0 and 100 are
  // allowed to latch exactly.
  static int shown = -1;
  int raw = battPercentRaw();
  if (shown < 0) { shown = raw; return (uint8_t)shown; }
  if (raw >= shown + 2 || raw <= shown - 2 || raw == 0 || raw == 100) shown = raw;
  return (uint8_t)shown;
}
bool battCharging() { return battVoltage() > 4.25f; }

// ---- compass
bool compassReadRaw(Vec3f& raw) {
#if ENABLE_COMPASS
  if (!s_compassOk) return false;
  s_magUse = millis();
  if (!s_magAwake) { if (s_ct == CT_QMC) initQMC(); else initHMC(); s_magAwake = true; delay(6); }
  uint8_t b[6];
  if (s_ct == CT_QMC) {
    if (!rBytes(ADDR_QMC5883L, 0x00, b, 6)) return false;
    raw.x = (int16_t)(b[0] | (b[1] << 8));
    raw.y = (int16_t)(b[2] | (b[3] << 8));
    raw.z = (int16_t)(b[4] | (b[5] << 8));
    return true;
  } else if (s_ct == CT_HMC) {
    if (!rBytes(ADDR_HMC5883L, 0x03, b, 6)) return false;
    raw.x = (int16_t)((b[0] << 8) | b[1]);
    raw.z = (int16_t)((b[2] << 8) | b[3]);
    raw.y = (int16_t)((b[4] << 8) | b[5]);
    return true;
  }
#endif
  return false;
}
float compassHeading() {
  Vec3f m;
  if (!compassReadRaw(m)) return -1;
  float ox = (S.magMin[0] + S.magMax[0]) * 0.5f;
  float oy = (S.magMin[1] + S.magMax[1]) * 0.5f;
  float x = m.x - ox, y = m.y - oy;
  float h = atan2f(y, x) * 57.29578f + S.magDeclination + S.magOffset;
  while (h < 0)   h += 360;
  while (h >= 360) h -= 360;
  return h;
}
void compassSetNorth() {
  // Point the device at true north, then call this: makes the current
  // reading become 0deg by baking the difference into magOffset.
  Vec3f m;
  if (!compassReadRaw(m)) return;
  float ox = (S.magMin[0] + S.magMax[0]) * 0.5f;
  float oy = (S.magMin[1] + S.magMax[1]) * 0.5f;
  float raw = atan2f(m.y - oy, m.x - ox) * 57.29578f + S.magDeclination;
  S.magOffset = -raw;
  settingsSave();
}
void compassCalReset() {
  for (int i = 0; i < 3; i++) { S.magMin[i] = 32767; S.magMax[i] = -32768; }
}
void compassCalSample() {
  Vec3f m;
  if (!compassReadRaw(m)) return;
  int16_t v[3] = { (int16_t)m.x, (int16_t)m.y, (int16_t)m.z };
  for (int i = 0; i < 3; i++) {
    if (v[i] < S.magMin[i]) S.magMin[i] = v[i];
    if (v[i] > S.magMax[i]) S.magMax[i] = v[i];
  }
}
void compassCalFinish() {
  S.compassCalibrated = true;
  settingsSave();
}

// ---- ToF
// Non-blocking: the sensor ranges continuously while something uses it and the
// latest sample is cached, so callers never stall the UI waiting ~33 ms for a
// measurement. It stops ranging (saves ~19 mA) 2 s after the last read.
static bool tofPoll() {
#if ENABLE_TOF
  if (!s_tofOk) return false;
  s_tofUse = millis();
  if (!s_tofRun) { tof.startContinuous(); s_tofRun = true; s_tofValid = false; return false; }
  if ((tof.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) == 0) return false;   // not ready yet
  uint16_t r = tof.readReg16Bit(VL53L0X::RESULT_RANGE_STATUS + 10);
  tof.writeReg(VL53L0X::SYSTEM_INTERRUPT_CLEAR, 0x01);
  s_tofValid = r < 8190 && r > 0; s_tofMm = r; s_tofAt = millis();
  return true;
#else
  return false;
#endif
}
bool tofReadMm(uint16_t& mm) {
  if (!s_tofOk) return false;
  tofPoll();
  if (!s_tofValid || millis() - s_tofAt > 250) return false;
  mm = s_tofMm;
  return true;
}
uint16_t tofReadAvg(uint8_t samples) {
  uint32_t sum = 0; uint8_t got = 0;
  uint32_t until = millis() + 60UL * samples + 100;
  while (got < samples && (int32_t)(until - millis()) > 0) {
    if (tofPoll() && s_tofValid) { sum += s_tofMm; got++; }
    else delay(2);
  }
  return got ? (uint16_t)(sum / got) : 0;
}

// ---- idle / sleep
void sensorsIdleTick() {
  uint32_t now = millis();
#if ENABLE_MPU
  if (s_mpuOk && s_mpuAwake && now - s_mpuUse > 3000) { mpu.enableSleep(true); s_mpuAwake = false; }
#endif
#if ENABLE_COMPASS
  if (s_compassOk && s_magAwake && now - s_magUse > 3000) {
    if (s_ct == CT_QMC) wReg(ADDR_QMC5883L, 0x09, 0x1C); else wReg(ADDR_HMC5883L, 0x02, 0x03);
    s_magAwake = false;
  }
#endif
#if ENABLE_TOF
  if (s_tofOk && s_tofRun && now - s_tofUse > 2000) { tof.stopContinuous(); s_tofRun = false; s_tofValid = false; }
#endif
}
void sensorsSleepAll() {
  s_mpuUse = s_magUse = s_tofUse = millis() - 100000;
  sensorsIdleTick();
}
