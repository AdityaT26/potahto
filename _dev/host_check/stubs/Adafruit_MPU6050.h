#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
enum mpu6050_accel_range_t { MPU6050_RANGE_2_G, MPU6050_RANGE_4_G, MPU6050_RANGE_8_G, MPU6050_RANGE_16_G };
enum mpu6050_gyro_range_t { MPU6050_RANGE_250_DEG, MPU6050_RANGE_500_DEG, MPU6050_RANGE_1000_DEG, MPU6050_RANGE_2000_DEG };
enum mpu6050_bandwidth_t { MPU6050_BAND_260_HZ, MPU6050_BAND_184_HZ, MPU6050_BAND_94_HZ, MPU6050_BAND_44_HZ, MPU6050_BAND_21_HZ, MPU6050_BAND_10_HZ, MPU6050_BAND_5_HZ };
class Adafruit_MPU6050 {
 public:
  bool begin(uint8_t = 0x68, TwoWire* = nullptr, int32_t = 0) { return true; }
  void setAccelerometerRange(mpu6050_accel_range_t) {}
  void setGyroRange(mpu6050_gyro_range_t) {}
  void setFilterBandwidth(mpu6050_bandwidth_t) {}
  bool enableSleep(bool) { return true; }
  bool getEvent(sensors_event_t* a, sensors_event_t* g, sensors_event_t* t) {
    a->acceleration = {0, 0, 9.81f}; g->gyro = {0, 0, 0}; t->temperature = 25; return true; }
};
