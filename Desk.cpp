// Alarms, countdown timer, pomodoro and PC stats (background engines)
#include "Multitool.h"
#include "Proto.h"
#include <time.h>

// ---- timer
static uint32_t s_timerEnd = 0; static bool s_timerOn = false;
void timerStart(uint16_t m) { s_timerEnd = millis() + (uint32_t)m * 60000UL; s_timerOn = true; }
void timerStop()            { s_timerOn = false; }
bool timerRunning()         { return s_timerOn; }
uint32_t timerLeftMs()      { return s_timerOn ? max<int32_t>(0, (int32_t)(s_timerEnd - millis())) : 0; }

// ---- alarms
static int s_lastAlarmKey = -1;          // day*1440+minute already fired
static bool dayOk(int i, int wday) { return S.alarmDays[i] == 0 || (S.alarmDays[i] >> ((wday + 6) % 7)) & 1; }
int alarmNextIndex(int* until) {
  if (!timeValid()) return -1;
  time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
  int best = -1, bestD = 1 << 30, nowM = lt.tm_hour * 60 + lt.tm_min;
  for (int i = 0; i < ALARMS; i++) {
    if (!S.alarmOn[i]) continue;
    for (int d = 0; d < 8; d++) {
      int wd = (lt.tm_wday + d) % 7, am = S.alarmH[i] * 60 + S.alarmM[i];
      if (d == 0 && am <= nowM) continue;
      if (!dayOk(i, wd)) continue;
      int delta = d * 1440 + am - nowM;
      if (delta < bestD) { bestD = delta; best = i; }
      break;
    }
  }
  if (until) *until = best >= 0 ? bestD : 0;
  return best;
}
static void alarmTick() {
  if (!timeValid()) return;
  time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
  int key = lt.tm_yday * 1440 + lt.tm_hour * 60 + lt.tm_min;
  if (key == s_lastAlarmKey) return;
  for (int i = 0; i < ALARMS; i++) {
    if (!S.alarmOn[i] || S.alarmH[i] != lt.tm_hour || S.alarmM[i] != lt.tm_min || !dayOk(i, lt.tm_wday)) continue;
    s_lastAlarmKey = key;
    char sub[20]; snprintf(sub, sizeof(sub), "%02d:%02d", lt.tm_hour, lt.tm_min);
    ringStart("Alarm", sub, 60000, TONE_ALARM);
    iotEvent("alarm", "ring");
    if (S.alarmDays[i] == 0) { S.alarmOn[i] = false; settingsSaveSoon(); }   // one-shot
    break;
  }
}

// ---- pomodoro
static PomPhase s_phase = POM_IDLE;
static bool     s_pomRun = false;
static uint32_t s_pomLeft = 0, s_pomStamp = 0;
static uint8_t  s_round = 1;
static uint16_t s_doneToday = 0;
static int      s_doneDay = -1;

static uint32_t phaseLen(PomPhase p) {
  return 60000UL * (p == POM_FOCUS ? S.pomWork : p == POM_SHORT ? S.pomShort : p == POM_LONG ? S.pomLong : 0);
}
PomPhase pomPhase()   { return s_phase; }
bool     pomRunning() { return s_pomRun; }
uint32_t pomPhaseMs() { return phaseLen(s_phase); }
uint8_t  pomRound()   { return s_round; }
uint16_t pomDoneToday() { return s_doneToday; }
uint32_t pomLeftMs() {
  if (!s_pomRun) return s_pomLeft;
  uint32_t el = millis() - s_pomStamp;
  return el >= s_pomLeft ? 0 : s_pomLeft - el;
}
static void enter(PomPhase p, bool run) {
  s_phase = p; s_pomLeft = phaseLen(p); s_pomStamp = millis(); s_pomRun = run;
  if (run) iotEvent("pomodoro", p == POM_FOCUS ? "focus" : "break");
}
void pomToggle() {
  if (s_phase == POM_IDLE) { s_round = 1; enter(POM_FOCUS, true); return; }
  if (s_pomRun) { s_pomLeft = pomLeftMs(); s_pomRun = false; }
  else { s_pomStamp = millis(); s_pomRun = true; if (s_pomLeft == phaseLen(s_phase)) iotEvent("pomodoro", s_phase == POM_FOCUS ? "focus" : "break"); }
}
static void advance(bool completed) {
  if (s_phase == POM_FOCUS) {
    if (completed) {
      time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
      if (lt.tm_yday != s_doneDay) { s_doneDay = lt.tm_yday; s_doneToday = 0; }
      s_doneToday++;
    }
    enter(s_round >= S.pomRounds ? POM_LONG : POM_SHORT, S.pomAuto);
  } else {
    if (s_phase == POM_LONG) s_round = 1; else s_round++;
    enter(POM_FOCUS, S.pomAuto);
  }
}
void pomSkip()  { if (s_phase != POM_IDLE) advance(false); }
void pomReset() { s_phase = POM_IDLE; s_pomRun = false; s_pomLeft = 0; s_round = 1; }
static void pomTick() {
  if (!s_pomRun || pomLeftMs() > 0) return;
  bool wasFocus = s_phase == POM_FOCUS;
  static const ToneStep up[]   = { { 1047, 120 }, { 1319, 120 }, { 1568, 240 } };
  static const ToneStep down[] = { { 1568, 120 }, { 1319, 120 }, { 1047, 240 } };
  static const uint16_t vib[] = { 200, 100, 200, 100, 400 };
  if (S.ntfMode >= 2) buzzSeq(wasFocus ? up : down, 3);
  if (S.ntfMode >= 1) motorPattern(vib, 5);
  npxFlash(wasFocus ? strip.Color(0, 200, 60) : strip.Color(220, 40, 0), 5, 250);
  notifyPush(wasFocus ? "Focus done" : "Break over", wasFocus ? "Time for a break" : "Back to focus", "pomodoro");
  if (wasFocus) iotEvent("pomodoro", "done");
  advance(true);
}

// ---- PC stats
static PcStats s_pc = {};
const PcStats& pcStats() { return s_pc; }
static bool cPcStats(JsonObjectConst q, JsonObject, ProtoCtx&) {
  s_pc.cpu = q["cpu"] | -1.0f;  s_pc.ram = q["ram"] | -1.0f;  s_pc.gpu = q["gpu"] | -1.0f;
  s_pc.cpuT = q["cpuT"] | -1.0f; s_pc.gpuT = q["gpuT"] | -1.0f; s_pc.disk = q["disk"] | -1.0f;
  s_pc.upKBs = q["up"] | -1.0f; s_pc.downKBs = q["down"] | -1.0f;
  strlcpy(s_pc.host, q["host"] | "PC", sizeof(s_pc.host));
  uint8_t c = (uint8_t)clampv<float>(s_pc.cpu, 0, 100);
  if (s_pc.histN < sizeof(s_pc.hist)) s_pc.hist[s_pc.histN++] = c;
  else { memmove(s_pc.hist, s_pc.hist + 1, sizeof(s_pc.hist) - 1); s_pc.hist[sizeof(s_pc.hist) - 1] = c; }
  s_pc.at = millis();
  return true;
}
static bool cDeskStatus(JsonObjectConst, JsonObject r, ProtoCtx&) {
  r["timeValid"] = timeValid();
  r["timerLeft"] = timerLeftMs() / 1000;
  static const char* PH[] = { "idle", "focus", "short break", "long break" };
  r["pom"]["phase"] = PH[s_phase]; r["pom"]["running"] = s_pomRun; r["pom"]["left"] = pomLeftMs() / 1000;
  r["pom"]["round"] = s_round; r["pom"]["today"] = s_doneToday;
  int until; int n = alarmNextIndex(&until);
  r["nextAlarm"] = n; r["nextAlarmIn"] = until;
  return true;
}
static bool cTimer(JsonObjectConst q, JsonObject, ProtoCtx&) {
  int m = q["minutes"] | 0;
  if (m > 0) { timerStart((uint16_t)clampv(m, 1, 600)); } else timerStop();
  return true;
}
static bool cPom(JsonObjectConst q, JsonObject, ProtoCtx&) {
  const char* a = q["action"] | "";
  if (!strcmp(a, "toggle")) pomToggle(); else if (!strcmp(a, "skip")) pomSkip(); else if (!strcmp(a, "reset")) pomReset();
  return true;
}

void deskBegin() {
  protoRegister("pc.stats",    cPcStats);
  protoRegister("desk.status", cDeskStatus);
  protoRegister("timer",       cTimer);
  protoRegister("pomodoro",    cPom);
}

void deskLoop() {
  static uint32_t s_lastSec = 0;
  uint32_t now = millis();
  if (s_timerOn && (int32_t)(now - s_timerEnd) >= 0) {
    s_timerOn = false;
    ringStart("Timer", "time is up", 45000, TONE_TIMER);
    iotEvent("timer", "done");
  }
  pomTick();
  if (now - s_lastSec >= 1000) { s_lastSec = now; alarmTick(); }
}
