// Clock: NTP, time pushed over USB / Bluetooth, manual setting
#include "Multitool.h"
#include <sys/time.h>
#include "esp_sntp.h"

static const char* s_src = "";
static uint32_t    s_setAt = 0;
static RTC_DATA_ATTR char s_rtcSrc[12];          // survives deep sleep / restart with the clock

static void ntpSynced(struct timeval*) { s_src = "WiFi"; s_setAt = millis(); strlcpy(s_rtcSrc, "WiFi", sizeof(s_rtcSrc)); }

void timeApplyTz() { setenv("TZ", S.tz[0] ? S.tz : "UTC0", 1); tzset(); }
void timeBegin() {
  timeApplyTz();
  sntp_set_time_sync_notification_cb(ntpSynced);
  if (timeValid()) s_src = s_rtcSrc[0] ? s_rtcSrc : "internal";
}
bool timeValid() { return time(nullptr) > 1704067200; }      // after 2024-01-01
bool timeNow(struct tm& lt) {
  if (!timeValid()) return false;
  time_t t = time(nullptr); localtime_r(&t, &lt); return true;
}
void timeSetEpoch(double epoch, const char* source) {
  if (epoch < 1704067200.0) return;
  double now = (double)time(nullptr);
  // a sync from WiFi (NTP) is better than the others; don't let a phone or a
  // PC with a slightly wrong clock fight it every few seconds
  bool better = !timeValid() || fabs(now - epoch) > 2.0;
  if (strcmp(s_src, "WiFi") == 0 && millis() - s_setAt < 3600000UL && strcmp(source, "WiFi") != 0) better = fabs(now - epoch) > 30.0;
  if (!better) { s_setAt = millis(); return; }
  struct timeval tv; tv.tv_sec = (time_t)epoch; tv.tv_usec = (suseconds_t)((epoch - (double)(time_t)epoch) * 1e6);
  settimeofday(&tv, nullptr);
  timeApplyTz();
  s_src = source; s_setAt = millis();
  strlcpy(s_rtcSrc, source, sizeof(s_rtcSrc));
}
void timeSetLocalS(int y, int mo, int d, int h, int mi, int sec, const char* source) {
  struct tm lt = {};
  lt.tm_year = y - 1900; lt.tm_mon = mo - 1; lt.tm_mday = d; lt.tm_hour = h; lt.tm_min = mi; lt.tm_sec = sec; lt.tm_isdst = -1;
  timeApplyTz();
  time_t t = mktime(&lt);
  if (strcmp(source, "Manual") != 0) { timeSetEpoch((double)t, source); return; }
  struct timeval tv = { t, 0 };
  settimeofday(&tv, nullptr);
  s_src = "Manual"; s_setAt = millis(); strlcpy(s_rtcSrc, "Manual", sizeof(s_rtcSrc));
}
void timeSetLocal(int y, int mo, int d, int h, int mi) { timeSetLocalS(y, mo, d, h, mi, 0, "Manual"); }
const char* timeSource() { return timeValid() ? (s_src[0] ? s_src : "internal") : ""; }
uint32_t timeSetAgoS() { return s_setAt ? (millis() - s_setAt) / 1000 : 0; }
void timeFmtHM(char* o, size_t n, const struct tm& lt) {
  if (S.clock24) snprintf(o, n, "%02d:%02d", lt.tm_hour, lt.tm_min);
  else snprintf(o, n, "%d:%02d", (lt.tm_hour + 11) % 12 + 1, lt.tm_min);
}
const char* timeAmPm(const struct tm& lt) { return S.clock24 ? "" : (lt.tm_hour < 12 ? "AM" : "PM"); }
