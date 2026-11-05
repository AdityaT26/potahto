#pragma once
struct sensors_vec_t { float x, y, z; };
struct sensors_event_t {
  union { sensors_vec_t acceleration; sensors_vec_t gyro; sensors_vec_t magnetic; };
  float temperature;
};
