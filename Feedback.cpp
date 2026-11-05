// Non-blocking motor / buzzer / neopixel
#include "Multitool.h"
#include "driver/gpio.h"

// ---- LEDC shims
// pwmMotor() takes a logical intensity: 0 = off, 255 = full. On this board the
// motor is driven by a PNP high-side transistor (active-LOW), so the physical
// duty is inverted: logical 0 -> pin HIGH (off), logical 255 -> pin LOW (on).
static inline void pwmMotor(uint8_t duty) {
#if MOTOR_ACTIVE_LOW
  uint8_t phys = 255 - duty;
#else
  uint8_t phys = duty;
#endif
#if LEDC_CORE3
  ledcWrite(PIN_MOTOR, phys);
#else
  ledcWrite(LEDC_CH_MOTOR, phys);
#endif
}
static inline void pwmBuzzer(uint16_t freq, uint8_t duty) {
  if (freq == 0 || duty == 0) {
#if LEDC_CORE3
    ledcWrite(PIN_BUZZER, 0);
#else
    ledcWrite(LEDC_CH_BUZZER, 0);
#endif
    return;
  }
#if LEDC_CORE3
  ledcChangeFrequency(PIN_BUZZER, freq, 8);
  ledcWrite(PIN_BUZZER, duty);
#else
  ledcSetup(LEDC_CH_BUZZER, freq, 8);
  ledcWrite(LEDC_CH_BUZZER, duty);
#endif
}
static inline uint8_t volToDuty(uint8_t vol /*0..10*/) {
  // A square wave is loudest at 50% duty (128/255). Perceived loudness of a
  // passive buzzer barely changes between ~25% and 50%, so the low end is
  // spread out more and the top steps all land near full drive.
  static const uint8_t D[11] = { 0, 4, 8, 14, 22, 34, 50, 70, 92, 112, 128 };
  return D[clampv<uint8_t>(vol, 0, 10)];
}
static inline uint8_t strToDuty(uint8_t str /*0..10*/) {
  str = clampv<uint8_t>(str, 0, 10);
  if (str == 0) return 0;
  return map(str, 1, 10, 95, 255);      // motor needs a minimum to spin
}
// Motor-duration scaling. Single taps scale by the % (unless they're long, >80ms
// -> treated as "special", kept unless motorDurAll). Patterns are the "special"
// long feedback -> only scaled when motorDurAll is on.
static inline uint16_t scaleDurSingle(uint16_t ms) {
  if (S.motorDurAll || ms <= 80) return (uint16_t)((uint32_t)ms * S.motorDurPct / 100);
  return ms;
}
static inline uint16_t scaleDurPattern(uint16_t ms) {
  return S.motorDurAll ? (uint16_t)((uint32_t)ms * S.motorDurPct / 100) : ms;
}

static uint16_t s_sirenF = 0;
// ---- buzzer sequence
#define MAX_SEQ 24
static ToneStep s_seq[MAX_SEQ];
static uint8_t  s_seqLen = 0, s_seqIdx = 0;
static uint8_t  s_seqDuty = 0;
static uint32_t s_seqStart = 0;

static void seqStartStep() {
  if (s_seqIdx >= s_seqLen) { pwmBuzzer(0, 0); s_seqLen = 0; return; }
  ToneStep& t = s_seq[s_seqIdx];
  pwmBuzzer(t.freq, t.freq ? s_seqDuty : 0);
  s_seqStart = millis();
}

void buzzStop() { s_seqLen = 0; s_seqIdx = 0; pwmBuzzer(0, 0); }

void buzzSeq(const ToneStep* steps, uint8_t count) {
  if (!S.buzzerEnabled || count == 0) return;
  if (count > MAX_SEQ) count = MAX_SEQ;
  memcpy(s_seq, steps, count * sizeof(ToneStep));
  s_seqLen  = count;
  s_seqIdx  = 0;
  s_seqDuty = volToDuty(S.buzzerVolume);
  seqStartStep();
}
void buzzTone(uint16_t freq, uint16_t durMs) {
  ToneStep s = { freq, durMs };
  buzzSeq(&s, 1);
}
void buzzToneVol(uint16_t freq, uint16_t durMs, uint8_t vol) {
  if (!S.buzzerEnabled) return;
  ToneStep s = { freq, durMs };
  memcpy(s_seq, &s, sizeof(s));
  s_seqLen = 1; s_seqIdx = 0;
  s_seqDuty = volToDuty(vol);
  seqStartStep();
}

// Continuous alarm tone. Unlike buzzToneVol this does not return early when the
// global buzzer master is off, and it does not schedule an auto-off step, so
// the tone is held with no restart gaps. It also cancels any in-flight SFX
// sequence so the sequence engine in fbUpdate() won't fight it or silence it.
void buzzAlarmTone(uint16_t freq, uint8_t vol) {
  s_seqLen = 0; s_seqIdx = 0;          // take the buzzer away from any SFX seq
  pwmBuzzer(freq, volToDuty(vol));
}
void buzzAlarmStop() {
  s_seqLen = 0; s_seqIdx = 0;
  pwmBuzzer(0, 0);
  s_sirenF = 0;
}
void buzzSiren(uint8_t vol) {
  s_seqLen = 0; s_seqIdx = 0;
  // ~3 sweeps per second from 0.7x to 1.35x the resonant frequency, then a
  // short hold right on it: crosses the loudest point twice per sweep.
  uint32_t ph = millis() % 330;
  float k = ph < 260 ? 0.70f + 0.65f * (ph / 260.0f) : 1.0f;
  uint16_t f = (uint16_t)(S.buzResHz * k);
  if (abs((int)f - (int)s_sirenF) < 25) return;          // don't hammer the LEDC timer
  s_sirenF = f;
  pwmBuzzer(f, volToDuty(vol < 1 ? 1 : vol));
}
void buzzTestTone(uint16_t freq) { s_seqLen = 0; pwmBuzzer(freq, 128); }

// ---- motor pattern
#define MAX_MOT 16
static uint16_t s_mot[MAX_MOT];
static uint8_t  s_motLen = 0, s_motIdx = 0, s_motDuty = 0;
static uint32_t s_motStart = 0;

void motorOff() { s_motLen = 0; s_motIdx = 0; pwmMotor(0); }

void motorPattern(const uint16_t* onOff, uint8_t count) {
  if (!S.motorEnabled || count == 0) return;
  if (count > MAX_MOT) count = MAX_MOT;
  for (uint8_t i = 0; i < count; i++) s_mot[i] = scaleDurPattern(onOff[i]);
  s_motLen = count; s_motIdx = 0;
  s_motDuty = strToDuty(S.motorStrength);
  s_motStart = millis();
  pwmMotor(s_motDuty);                 // step 0 is always "on"
}
void motorPulseStr(uint16_t ms, uint8_t str) {
  if (!S.motorEnabled) return;
  s_mot[0] = scaleDurSingle(ms); s_motLen = 1; s_motIdx = 0;
  s_motDuty = strToDuty(str);
  s_motStart = millis();
  pwmMotor(s_motDuty);
}
void motorPulse(uint16_t ms) { motorPulseStr(ms, S.motorStrength); }

// ---- neopixel
static NpxMode  s_base = NPX_OFF;
static uint32_t s_baseColor = 0;
static uint8_t  s_baseSpeed = 5;
static bool     s_flashOn = false;
static uint32_t s_flashColor = 0;
static uint8_t  s_flashTimes = 0;
static uint16_t s_flashPeriod = 120;
static uint32_t s_flashStart = 0;

uint32_t npxHSV(uint8_t hue, uint8_t sat, uint8_t val) {
  return strip.gamma32(strip.ColorHSV((uint16_t)hue * 257, sat, val));
}
void npxSetBase(NpxMode mode, uint32_t color, uint8_t speed) {
  s_base = mode; s_baseColor = color; s_baseSpeed = clampv<uint8_t>(speed, 1, 10);
}
void npxSolid(uint32_t color) { s_base = NPX_SOLID; s_baseColor = color; }
void npxOff() { s_base = NPX_OFF; }
void npxFlash(uint32_t color, uint8_t times, uint16_t periodMs) {
  s_flashOn = true; s_flashColor = color; s_flashTimes = times;
  s_flashPeriod = periodMs ? periodMs : 100; s_flashStart = millis();
}

static uint32_t s_keepUntil = 0; static uint8_t s_keepBright = 0;
void npxKeepOn() { s_keepUntil = millis() + 250; s_keepBright = 0; }
void npxKeepOnBright(uint8_t b) { s_keepUntil = millis() + 250; s_keepBright = b; }
static uint32_t s_lastShown = 0xFFFFFFFF; static uint8_t s_lastBright = 0;
static void npxShow(uint32_t color, uint8_t bright) {
  if (color == s_lastShown && bright == s_lastBright) return;     // skip identical RMT writes
  s_lastShown = color; s_lastBright = bright;
  strip.setBrightness(bright); strip.setPixelColor(0, color); strip.show();
}
static void renderNeopixel() {
  if (!S.neopixelEnabled) { npxShow(0, 0); return; }
  uint32_t color = 0;
  uint8_t  bright = S.neopixelBright;
  uint32_t now = millis();
  bool keep = (int32_t)(s_keepUntil - now) >= 0;
  bool idle = powerLedIdle() && !keep;
  uint8_t base = (keep && s_keepBright) ? s_keepBright : S.neopixelBright;
  bright = base;

  if (s_flashOn) {
    uint32_t el = now - s_flashStart;
    uint32_t cyc = el / s_flashPeriod;
    if (cyc >= s_flashTimes) { s_flashOn = false; }
    else {
      bool on = (el % s_flashPeriod) < (uint32_t)(s_flashPeriod / 2);
      color = on ? s_flashColor : 0;
      npxShow(color, bright);
      return;
    }
  }

  switch (s_base) {
    case NPX_OFF:   color = 0; break;
    case NPX_SOLID: color = s_baseColor; break;
    case NPX_BLINK: {
      uint16_t per = map(s_baseSpeed, 1, 10, 900, 120);
      color = ((now % per) < per / 2) ? s_baseColor : 0;
    } break;
    case NPX_PULSE: {
      uint16_t per = map(s_baseSpeed, 1, 10, 1200, 300);
      float ph = (now % per) / (float)per;                  // 0..1
      float k  = ph < 0.25f ? (ph / 0.25f) : (ph < 0.5f ? (1 - (ph - 0.25f) / 0.25f) : 0);
      bright = (uint8_t)(base * k);
      color = s_baseColor;
    } break;
    case NPX_BREATHE: {
      uint16_t per = map(s_baseSpeed, 1, 10, 4200, 700);
      float ph = (now % per) / (float)per;
      float k  = 0.10f + 0.90f * (0.5f - 0.5f * cosf(ph * 2 * PI));
      bright = (uint8_t)(base * k);
      color = s_baseColor;
    } break;
    case NPX_RAINBOW: {
      uint16_t hue = (uint16_t)((now * (uint32_t)s_baseSpeed) & 0xFFFF);
      color = strip.gamma32(strip.ColorHSV(hue, 255, 255));
    } break;
  }
  if (idle) color = 0;
  npxShow(color, bright);
}
void npxForceRefresh() { s_lastShown = 0xFFFFFFFF; renderNeopixel(); }

// ---- background music
static const MusicNote* s_mus = nullptr;
static uint16_t s_musLen = 0, s_musIdx = 0;
static uint32_t s_musStart = 0;
static bool     s_musOn = false;
static uint8_t  s_musFlags = 0;

void musicPlayEx(const MusicNote* m, uint16_t n, uint8_t flags) {
  if (s_musOn && s_seqLen == 0) pwmBuzzer(0, 0);
  s_mus = n ? m : nullptr; s_musLen = n; s_musIdx = 0; s_musOn = false; s_musFlags = flags;
}
void musicPlay(const MusicNote* m, uint16_t n) { musicPlayEx(m, n, MUS_LOOP | MUS_GAME); }
void musicStop() { s_mus = nullptr; if (s_musOn && s_seqLen == 0) pwmBuzzer(0, 0); s_musOn = false; }
bool musicPlaying() { return s_mus != nullptr; }
uint16_t musicIndex() { return s_musIdx; }

// ---- master update
void fbUpdate() {
  uint32_t now = millis();
  // buzzer SFX / feedback sequence
  if (s_seqLen && (now - s_seqStart) >= s_seq[s_seqIdx].dur) {
    s_seqIdx++;
    seqStartStep();
  }
  // motor
  if (s_motLen) {
    if ((now - s_motStart) >= s_mot[s_motIdx]) {
      s_motIdx++;
      if (s_motIdx >= s_motLen) { motorOff(); }
      else { s_motStart = now; pwmMotor((s_motIdx & 1) ? 0 : s_motDuty); }
    }
  }
  // background music (plays only when the buzzer is free of SFX)
  bool musicAllowed = s_mus && ((s_musFlags & MUS_LOUD) || S.buzzerEnabled) && (!(s_musFlags & MUS_GAME) || S.gameMusic);
  if (!musicAllowed) {
    if (s_musOn) { if (s_seqLen == 0) pwmBuzzer(0, 0); s_musOn = false; }
  } else if (s_seqLen) {
    s_musOn = false;                       // yield: SFX owns the buzzer
  } else {
    if (!s_musOn) {
      const MusicNote& nt = s_mus[s_musIdx];
      uint8_t duty = (s_musFlags & MUS_LOUD) ? 128 : (s_musFlags & MUS_GAME) ? (uint8_t)(volToDuty(S.buzzerVolume) * 3 / 5) : volToDuty(S.buzzerVolume);
      pwmBuzzer(nt.freq, nt.freq ? duty : 0);
      s_musStart = now; s_musOn = true;
    } else if (now - s_musStart >= s_mus[s_musIdx].dur) {
      pwmBuzzer(0, 0);
      s_musIdx++;
      if (s_musIdx >= s_musLen) {
        if (s_musFlags & MUS_LOOP) s_musIdx = 0;
        else { s_mus = nullptr; s_musIdx = 0; }
      }
      s_musOn = false;
    }
  }
  renderNeopixel();
}

void fbBegin() {
  // Park the motor in its OFF state before LEDC takes over the pin, so a
  // PNP high-side driver doesn't twitch at boot.
  pinMode(PIN_MOTOR, OUTPUT);
  digitalWrite(PIN_MOTOR, MOTOR_ACTIVE_LOW ? HIGH : LOW);
#if LEDC_CORE3
  ledcAttach(PIN_MOTOR, LEDC_MOTOR_FREQ, LEDC_MOTOR_RES);
  ledcAttach(PIN_BUZZER, 2000, 8);
#else
  ledcSetup(LEDC_CH_MOTOR, LEDC_MOTOR_FREQ, LEDC_MOTOR_RES);
  ledcAttachPin(PIN_MOTOR, LEDC_CH_MOTOR);
  ledcSetup(LEDC_CH_BUZZER, 2000, 8);
  ledcAttachPin(PIN_BUZZER, LEDC_CH_BUZZER);
#endif
  pwmMotor(0);
  pwmBuzzer(0, 0);
  // Strongest GPIO drive (~40 mA) for a buzzer wired straight to the pin:
  // the default (~20 mA) leaves a magnetic buzzer noticeably quieter.
  gpio_set_drive_capability((gpio_num_t)PIN_BUZZER, GPIO_DRIVE_CAP_3);
  strip.begin();
  strip.setBrightness(S.neopixelBright);
  strip.clear();
  strip.show();
}

// ---- semantic events
void fbNav()  { buzzTone(2400, 9);  motorPulseStr(8, 2); }
void fbEdge() { buzzTone(200, 55);  motorPulseStr(35, 5); npxFlash(strip.Color(60,0,0), 1, 80); }
void fbSelect() {
  static const ToneStep s[] = {{1568,26},{2093,42}};
  buzzSeq(s, 2); motorPulseStr(16, 6); npxFlash(strip.Color(0,120,160), 1, 130);
}
void fbBack() {
  static const ToneStep s[] = {{1568,22},{1047,36}};
  buzzSeq(s, 2); motorPulseStr(12, 4);
}
void fbConfirm() {
  static const ToneStep s[] = {{1568,40},{2093,40},{2637,90}};
  buzzSeq(s, 3);
  static const uint16_t m[] = {24, 40, 24};
  motorPattern(m, 3);
  npxFlash(strip.Color(0,180,40), 2, 130);
}
void fbCancel() {
  static const ToneStep s[] = {{880,40},{587,60}};
  buzzSeq(s, 2); motorPulseStr(20, 4);
}
void fbError() {
  static const ToneStep s[] = {{320,120},{240,170}};
  buzzSeq(s, 2);
  static const uint16_t m[] = {60, 55, 60};
  motorPattern(m, 3);
  npxFlash(strip.Color(200,0,0), 3, 110);
}
void fbToastCue() { buzzTone(2600, 9); }
void fbBoot() {
  if (S.startupSound && !(S.buzzerEnabled && tonePlay(TONE_BOOT, false))) {
    static const ToneStep s[] = {{1047,90},{1319,90},{1568,90},{2093,170}};
    buzzSeq(s, 4);
  }
  static const uint16_t m[] = {30, 40, 30, 40, 60};
  motorPattern(m, 5);
}
