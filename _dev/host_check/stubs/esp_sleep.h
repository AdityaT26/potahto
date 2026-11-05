#pragma once
#include <stdint.h>
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_EXT0 = 2, ESP_SLEEP_WAKEUP_EXT1, ESP_SLEEP_WAKEUP_TIMER } esp_sleep_wakeup_cause_t;
typedef enum { ESP_EXT1_WAKEUP_ANY_LOW = 0, ESP_EXT1_WAKEUP_ANY_HIGH = 1 } esp_sleep_ext1_wakeup_mode_t;
typedef enum { ESP_PD_DOMAIN_RTC_PERIPH = 0 } esp_sleep_pd_domain_t;
typedef enum { ESP_PD_OPTION_OFF, ESP_PD_OPTION_ON, ESP_PD_OPTION_AUTO } esp_sleep_pd_option_t;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_UNDEFINED; }
inline int esp_sleep_enable_ext1_wakeup(uint64_t, esp_sleep_ext1_wakeup_mode_t) { return 0; }
inline int esp_sleep_enable_timer_wakeup(uint64_t) { return 0; }
inline int esp_sleep_pd_config(esp_sleep_pd_domain_t, esp_sleep_pd_option_t) { return 0; }
void potahto_fake_deep_sleep();
inline void esp_deep_sleep_start() { potahto_fake_deep_sleep(); }
