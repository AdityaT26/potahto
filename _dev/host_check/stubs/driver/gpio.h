#pragma once
typedef int gpio_num_t;
typedef enum { GPIO_DRIVE_CAP_0, GPIO_DRIVE_CAP_1, GPIO_DRIVE_CAP_2, GPIO_DRIVE_CAP_3 } gpio_drive_cap_t;
inline int gpio_set_drive_capability(gpio_num_t, gpio_drive_cap_t) { return 0; }
inline int gpio_hold_en(gpio_num_t) { return 0; }
inline int gpio_hold_dis(gpio_num_t) { return 0; }
inline void gpio_deep_sleep_hold_en() {}
inline void gpio_deep_sleep_hold_dis() {}
