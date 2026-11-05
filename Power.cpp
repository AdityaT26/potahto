// Screen off / dim, lock screen, deep sleep
#include "Multitool.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#define OLED_CONTRAST   0xCF
#define LOCKSCREEN_OFF_MS 10000UL     // a lock screen nobody touches goes dark quickly

static PowerState s_state = PWR_ON;
static bool       s_locked = false, s_forceLock = false;
static uint32_t   s_offSince = 0, s_onSince = 0, s_peekUntil = 0;
static uint8_t    s_keepPrev = 0, s_keepCur = 0;
static bool       s_fast = false, s_alarmWake = false;
static const char* s_reason = "power on";
static char       s_code[8]; static uint8_t s_codeLen = 0;
static uint32_t   s_codeErrAt = 0;
static bool       s_lowCpu = false;

static void setCpu(bool low) {
  if (low == s_lowCpu) return;
  s_lowCpu = low;
  setCpuFrequencyMhz(low ? 80 : 240);
}

// ---- boot
static const gpio_num_t HELD[] = { (gpio_num_t)PIN_MOTOR, (gpio_num_t)PIN_BUZZER, (gpio_num_t)PIN_NEOPIXEL, (gpio_num_t)PIN_IR_TX };
void powerBegin() {
  esp_sleep_wakeup_cause_t c = esp_sleep_get_wakeup_cause();
  for (gpio_num_t p : HELD) gpio_hold_dis(p);
  gpio_deep_sleep_hold_dis();
  if (c == ESP_SLEEP_WAKEUP_EXT1)  { s_fast = true; s_reason = "button"; }
  if (c == ESP_SLEEP_WAKEUP_TIMER) { s_fast = true; s_reason = "alarm"; s_alarmWake = true; }
  s_onSince = millis();
}
bool powerFastBoot() { return s_fast; }
const char* powerWakeReason() { return s_reason; }

// ---- state changes
static bool lockDue() {
  if (S.lockMode == 0) return false;
  if (S.lockMode == 2 && strlen(S.lockCode) < 4) { if (!s_forceLock) return false; }
  if (s_forceLock) return true;
  return s_state == PWR_OFF && millis() - s_offSince >= (uint32_t)S.lockDelayM * 60000UL;
}
static void screenOn() {
  if (s_state == PWR_OFF) u8g2.setPowerSave(0);
  u8g2.setContrast(OLED_CONTRAST);
  s_state = PWR_ON; s_onSince = millis();
  setCpu(false);
  gfxResetShadow();
  inputMarkActivity();
}
void powerScreenOff(bool lockNow) {
  if (lockNow && S.lockMode) s_forceLock = true;
  if (s_state == PWR_OFF) return;
  u8g2.setPowerSave(1);
  s_state = PWR_OFF; s_offSince = millis(); s_peekUntil = 0;
  s_codeLen = 0;
  npxForceRefresh();
}
void powerWake() {
  if (s_state != PWR_OFF) { if (s_state == PWR_DIM) screenOn(); return; }
  if (lockDue()) { s_locked = true; s_codeLen = 0; }
  s_forceLock = false;
  screenOn();
}
void powerPeek(uint32_t ms) {
  bool wasOff = s_state == PWR_OFF;
  powerWake();
  if (wasOff) s_peekUntil = millis() + ms;
}
void powerLockNow() { if (S.lockMode == 0) { toast("Lock screen is off"); return; } s_forceLock = true; powerScreenOff(true); }
PowerState powerState()   { return s_state; }
bool powerScreenOn()      { return s_state != PWR_OFF; }
bool powerLocked()        { return s_locked; }
void powerKeepAwake(uint8_t level) { if (level > s_keepCur) s_keepCur = level; }
bool powerLedIdle() {
  if (s_state == PWR_OFF) return true;
  return S.npxOffS && millis() - lastActivityMs() > (uint32_t)S.npxOffS * 1000UL;
}

static bool appIsLive() { return (APPS[currentApp()].flags & APPF_LIVE) != 0; }

static bool canDeepSleep() {
  if (watchBusy() || timerRunning() || pomRunning() || ringActive() || hidBusy() || otaActive()) return false;
  if (hidUsbMounted()) return false;              // on a PC: powered, and you may want the keys
  if (S.wifiMode == WM_ALWAYS) return false;      // asked to stay online
  return true;
}

// ---- lock screen input
static void lockInput() {
  bool any = false;
  for (uint8_t b = 0; b < BTN_COUNT; b++) if (btnPressed(b)) any = true;
  if (S.lockMode != 2 || strlen(S.lockCode) < 4) {
    if (any) { s_locked = false; fbSelect(); inputClearFrame(); }
    return;
  }
  static const char L[4] = { 'U', 'D', 'L', 'R' };
  for (uint8_t b = 0; b < 4; b++) if (btnPressed(b) && s_codeLen < sizeof(s_code) - 1) {
    s_code[s_codeLen++] = L[b]; s_code[s_codeLen] = 0; fbNav();
  }
  if (btnPressed(BTN_SELECT)) { s_codeLen = 0; fbCancel(); }
  size_t need = strlen(S.lockCode);
  if (s_codeLen >= need) {
    if (strncasecmp(s_code, S.lockCode, need) == 0) { s_locked = false; fbConfirm(); }
    else { fbError(); s_codeErrAt = millis(); }
    s_codeLen = 0;
  }
  inputClearFrame();
}

// ---- main hook
void powerLoop() {
  uint32_t now = millis();
  s_keepPrev = s_keepCur; s_keepCur = 0;
  bool anyPress = false;
  for (uint8_t b = 0; b < BTN_COUNT; b++) if (btnPressed(b)) anyPress = true;

  // power gesture: keep holding SELECT
  if (btnPowerGesture()) {
    if (s_state == PWR_OFF) powerWake(); else powerScreenOff(false);
    inputClearFrame(); fbBack();
    return;
  }

  if (s_state == PWR_OFF) {
    if (anyPress) {
      bool willLock = lockDue();
      powerWake();
      if (willLock || !appIsLive()) inputClearFrame();   // this press only wakes
      return;
    }
    // automatic deep sleep
    uint32_t sleepMs = s_alarmWake ? 120000UL : (uint32_t)S.sleepAfterM * 60000UL;
    if (sleepMs && now - s_offSince > sleepMs && canDeepSleep()) powerDeepSleep();
    setCpu(!appIsLive());
    return;
  }
  s_alarmWake = s_alarmWake && !anyPress;          // a real user took over

  if (s_locked) {
    lockInput();
    if (now - lastActivityMs() > LOCKSCREEN_OFF_MS && now - s_onSince > LOCKSCREEN_OFF_MS && !ringActive()) powerScreenOff(false);
    return;
  }

  // peek (notification lit the screen): back off if nobody touched it
  if (s_peekUntil && (int32_t)(now - s_peekUntil) >= 0) {
    s_peekUntil = 0;
    if (lastActivityMs() < s_onSince + 50) { powerScreenOff(false); return; }
  }

  // timeouts
  if (!S.scrOffS || s_keepPrev >= 2 || ringActive() || modalActive()) {
    if (s_state == PWR_DIM) screenOn();
    return;
  }
  uint32_t idle = now - max(lastActivityMs(), s_onSince);
  uint32_t offMs = (uint32_t)S.scrOffS * 1000UL * (s_keepPrev == 1 ? 4 : 1);
  uint32_t dimMs = offMs > 15000 ? offMs - 10000 : offMs * 2 / 3;
  if (idle >= offMs) { powerScreenOff(false); return; }
  if (idle >= dimMs) {
    if (s_state != PWR_DIM) { u8g2.setContrast(1); s_state = PWR_DIM; }
  } else if (s_state == PWR_DIM) screenOn();
}

// ---- drawing
void powerDrawHoldHint() {
  float p = selectHoldProgress();
  if (p <= 0) return;
  u8g2.setDrawColor(0); u8g2.drawBox(0, 51, 128, 13); u8g2.setDrawColor(1);
  u8g2.drawRFrame(0, 51, 128, 13, 3);
  u8g2.setFont(F_TINY);
  u8g2.drawStr(4, 60, "let go: back");
  u8g2.drawFrame(58, 55, 40, 5);
  u8g2.drawBox(59, 56, (int)(38 * p), 3);
  u8g2.drawStr(101, 60, "sleep");
}

void powerDrawLock() {
  struct tm lt;
  bool tv = timeNow(lt);
  // status row
  uiBatteryIcon(113, 1);
  u8g2.setFont(F_TINY);
  char b[24]; snprintf(b, sizeof(b), "%u%%", battPercent()); uiRight(110, 7, b);
  int x = 2;
  if (g_bleConnected) { u8g2.drawStr(x, 7, "BT"); x += 11; }
  if (netState() != NET_OFF) { uiWifiIcon(x, 0); x += 12; }
  int un = notifyUnread();
  if (un) { u8g2.drawFrame(x, 2, 7, 5); u8g2.drawLine(x, 2, x + 3, 4); u8g2.drawLine(x + 6, 2, x + 3, 4); snprintf(b, sizeof(b), "%d", un); u8g2.drawStr(x + 9, 7, b); }
  // time
  if (tv) {
    char hm[8]; timeFmtHM(hm, sizeof(hm), lt);
    u8g2.setFont(F_HUGE);
    int w = u8g2.getStrWidth(hm);
    const char* ap = timeAmPm(lt);
    u8g2.setFont(F_SMALL); int aw = ap[0] ? u8g2.getStrWidth(ap) + 2 : 0;
    int x0 = (128 - w - aw) / 2;
    u8g2.setFont(F_HUGE); u8g2.drawStr(x0, 41, hm);
    if (aw) { u8g2.setFont(F_SMALL); u8g2.drawStr(x0 + w + 2, 41, ap); }
    strftime(b, sizeof(b), "%A %d %B", &lt);
    u8g2.setFont(F_SMALL); uiCenter(51, b);
  } else {
    u8g2.setFont(F_BIG); uiCenter(38, "--:--");
    u8g2.setFont(F_TINY); uiCenter(49, "set the time: Settings > Time");
  }
  // one status line: ring > timer > pomodoro > guard > next alarm
  char st[32] = "";
  int until; int al = alarmNextIndex(&until);
  if (timerRunning()) { uint32_t s = timerLeftMs() / 1000; snprintf(st, sizeof(st), "timer %lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60)); }
  else if (pomRunning()) { uint32_t s = pomLeftMs() / 1000; snprintf(st, sizeof(st), "%s %lu:%02lu", pomPhase() == POM_FOCUS ? "focus" : "break", (unsigned long)(s / 60), (unsigned long)(s % 60)); }
  else if (watchBusy()) snprintf(st, sizeof(st), "%s", watchStatusLine());
  else if (al >= 0) {
    struct tm at = {}; at.tm_hour = S.alarmH[al]; at.tm_min = S.alarmM[al];
    char hm[8]; timeFmtHM(hm, sizeof(hm), at); snprintf(st, sizeof(st), "alarm %s%s", hm, timeAmPm(at));
  }
  u8g2.setFont(F_TINY);
  // bottom: code dots or hint
  if (S.lockMode == 2 && strlen(S.lockCode) >= 4) {
    int need = strlen(S.lockCode), w = need * 9;
    int x1 = 127 - w;
    bool err = millis() - s_codeErrAt < 500;
    int shake = err ? ((millis() / 40) & 1 ? 2 : -2) : 0;
    for (int i = 0; i < need; i++) {
      int cx = x1 + i * 9 + 4 + shake;
      if (i < s_codeLen) u8g2.drawDisc(cx, 59, 3); else u8g2.drawCircle(cx, 59, 3);
    }
    u8g2.drawStr(1, 62, st[0] ? st : "enter code");
  } else {
    if (st[0]) u8g2.drawStr(1, 62, st);
    uiRight(127, 62, "any key");
  }
}

// ---- deep sleep
[[noreturn]] void powerDeepSleep() {
  // let go of the buttons first (a held wake button would wake it at once)
  uint32_t t0 = millis();
  while (anyButtonDown() && millis() - t0 < 3000) { inputUpdate(); delay(10); }
  settingsSave();
  ringStop(); motorOff(); buzzStop(); musicStop(); hidCancel();
  if (S.wifiMode != WM_OFF) { /* the radio is torn down by deep sleep itself */ }
  // goodbye card
  u8g2.setPowerSave(0); u8g2.setContrast(OLED_CONTRAST);
  u8g2.setDisplayRotation(U8G2_R0);
  u8g2.clearBuffer();
  uiGlyph(GLYPH_POWER, 48, 4, 32);
  u8g2.setFont(F_UI); uiCenter(48, "Deep sleep");
  u8g2.setFont(F_TINY); uiCenter(60, "UP, DOWN or LEFT wakes me");
  u8g2.sendBuffer();
  npxOff(); strip.clear(); strip.show();
  delay(900);
  u8g2.setPowerSave(1);
  sensorsSleepAll();
  // park the outputs: motor (PNP, active LOW) must stay HIGH, the rest LOW
#if LEDC_CORE3
  ledcDetach(PIN_MOTOR); ledcDetach(PIN_BUZZER);
#endif
  pinMode(PIN_MOTOR, OUTPUT);   digitalWrite(PIN_MOTOR, MOTOR_ACTIVE_LOW ? HIGH : LOW);
  pinMode(PIN_BUZZER, OUTPUT);  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_NEOPIXEL, OUTPUT);digitalWrite(PIN_NEOPIXEL, LOW);
  pinMode(PIN_IR_TX, OUTPUT);   digitalWrite(PIN_IR_TX, LOW);
  for (gpio_num_t p : HELD) gpio_hold_en(p);
  gpio_deep_sleep_hold_en();
  // wake sources: UP / DOWN / LEFT (RTC-capable pins) pulled up, active LOW
  const gpio_num_t W[3] = { (gpio_num_t)PIN_BTN_UP, (gpio_num_t)PIN_BTN_DOWN, (gpio_num_t)PIN_BTN_LEFT };
  uint64_t mask = 0;
  for (gpio_num_t p : W) { rtc_gpio_init(p); rtc_gpio_set_direction(p, RTC_GPIO_MODE_INPUT_ONLY); rtc_gpio_pullup_en(p); rtc_gpio_pulldown_dis(p); mask |= 1ULL << p; }
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   // keep the pull-ups
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  int until;
  if (alarmNextIndex(&until) >= 0 && until > 0) {
    struct tm lt; uint64_t secs = (uint64_t)until * 60;
    if (timeNow(lt)) secs -= lt.tm_sec;          // land at the top of the minute
    if (secs > 25) secs -= 20; else secs = 5;
    esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
  }
  Serial.flush();
  esp_deep_sleep_start();
  for (;;) {}
}

void powerRestart() {
  settingsSave();
  u8g2.clearBuffer(); u8g2.setFont(F_UI); uiCenter(36, "Restarting..."); u8g2.sendBuffer();
  delay(300);
  ESP.restart();
  for (;;) {}
}
