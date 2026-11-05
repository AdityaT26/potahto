#pragma once
struct timeval;
typedef void (*sntp_sync_time_cb_t)(struct timeval*);
inline void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t) {}
