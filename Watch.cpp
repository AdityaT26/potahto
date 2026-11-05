// Tripwire + Desk Guard engines (background, screen-off safe)
#include "Multitool.h"
#include "Proto.h"

static uint16_t s_mm = 0; static bool s_ok = false;
uint16_t watchDistanceMm() { return s_mm; }
bool     watchDistanceOk() { return s_ok; }

// ---- tripwire
static bool s_tArmed = false, s_tTripped = false, s_tSilenced = false, s_tSiren = false;
static uint32_t s_tAt = 0, s_tMot = 0, s_tArmAt = 0;
bool tripArmed()   { return s_tArmed; }
bool tripTripped() { return s_tTripped; }
static void tripStopOutputs() { if (s_tSiren) buzzAlarmStop(); s_tSiren = false; motorOff(); }
void tripSilence() { s_tSilenced = true; tripStopOutputs(); }
void tripArm(bool on) {
  if (on == s_tArmed) return;
  s_tArmed = on; s_tArmAt = millis();
  if (!on) {
    if (s_tTripped) { iotEvent("tripwire", "cleared"); iotState("tripwire", false); }
    s_tTripped = false; tripStopOutputs();
  }
  iotEvent("tripwire", on ? "armed" : "disarmed");
}
static void tripTick() {
  if (!s_tArmed) return;
  uint32_t now = millis();
  bool broken = s_ok && s_mm < S.tripDistMm && now - s_tArmAt > 1500;   // 1.5 s to step away after arming
  if (broken && !s_tTripped) {
    bool wasDark = !powerScreenOn();
    s_tTripped = true; s_tSilenced = false; s_tAt = now;
    iotEvent("tripwire", "tripped"); iotState("tripwire", true);
    notifyPush("Tripwire", "Something crossed the beam", "trip");
    if (wasDark && S.lockMode == 2 && strlen(S.lockCode) >= 4) powerLockNow();   // left guarding: only the code silences it
    powerWake();
  } else if (!broken && s_tTripped && S.tripAutoReset && now - s_tAt > 3000) {
    s_tTripped = false; iotEvent("tripwire", "cleared"); iotState("tripwire", false);
    tripStopOutputs();
  }
  if (s_tTripped && !s_tSilenced) {
    // Loud siren: full drive, a fast sweep through the buzzer's resonant
    // frequency (Settings > Sound > Loudest tone). A passive buzzer is several
    // times louder at resonance than off it, so sweeping across it is the
    // loudest thing a GPIO-driven buzzer can do.
    if (S.tripUseBuzzer) { buzzSiren(S.tripVolume); s_tSiren = true; }
    else if (s_tSiren) { buzzAlarmStop(); s_tSiren = false; }
    if (S.tripUseMotor && now - s_tMot > 260) { s_tMot = now; motorPulseStr(160, 10); }
    if (S.tripUseLed) { npxSetBase(NPX_BLINK, strip.Color(255, 0, 0), 9); npxKeepOn(); }
  }
}

// ---- desk guard
static bool s_gArmed = false, s_gPresent = true, s_gIntruder = false;
static uint32_t s_gAwaySince = 0, s_gClearSince = 0, s_gNearAt = 0;
static char s_gLast[28] = "";
bool       guardArmed() { return s_gArmed; }
const char* guardLastAction() { return s_gLast; }
GuardState guardState() {
  if (!s_gArmed) return GS_OFF;
  if (S.dgMode == 1) return s_gIntruder ? GS_INTRUDER : GS_WATCHING;
  if (!s_gPresent) return GS_AWAY;
  return s_gAwaySince ? GS_LEAVING : GS_PRESENT;
}
uint32_t guardLeavingMs() {
  if (!s_gAwaySince) return 0;
  uint32_t el = millis() - s_gAwaySince, full = (uint32_t)S.dgDelayS * 1000UL;
  return el >= full ? 0 : full - el;
}
static void runAction(const char* macro, const char* what) {
  if (!macro || !macro[0]) { snprintf(s_gLast, sizeof(s_gLast), "%s (no action)", what); return; }
  char err[48];
  if (!hidReady()) { snprintf(s_gLast, sizeof(s_gLast), "%s: PC not connected", what); return; }
  if (hidRunMacro(macro, err, sizeof(err))) snprintf(s_gLast, sizeof(s_gLast), "%s: sent", what);
  else snprintf(s_gLast, sizeof(s_gLast), "%s: %s", what, err);
}
void guardArm(bool on) {
  if (on == s_gArmed) return;
  s_gArmed = on; s_gPresent = true; s_gIntruder = false; s_gAwaySince = 0; s_gClearSince = millis();
  if (on) { hidBleStart(); iotState("presence", true); }
  iotEvent("desk", on ? "guard_on" : "guard_off");
}
static void guardTick() {
  if (!s_gArmed) return;
  uint32_t now = millis();
  if (S.dgMode == 1) {                                   // watch behind me
    bool near = s_ok && s_mm < (uint16_t)S.dgNearCm * 10;
    if (near) {
      if (!s_gIntruder && now - s_gClearSince > 2000) {  // 2 s grace after arming
        s_gIntruder = true;
        runAction(S.dgNearM, "someone came");
        iotEvent("desk", "someone");
        motorPulseStr(60, 6);
      }
      s_gNearAt = now;
    } else if (s_gIntruder && now - s_gNearAt > 3000) s_gIntruder = false;   // re-arms after 3 s clear
    return;
  }
  uint16_t thr = (uint16_t)S.dgDistCm * 10;
  bool near = s_ok && s_mm < (s_gPresent ? thr + thr / 10 : thr);    // 10% hysteresis
  if (s_gPresent && !near) {
    if (!s_gAwaySince) s_gAwaySince = now;
    if (now - s_gAwaySince >= (uint32_t)S.dgDelayS * 1000UL) {
      s_gPresent = false;
      runAction(S.dgLeaveM, "you left");
      iotEvent("desk", "left"); iotState("presence", false);
    }
  } else if (near) s_gAwaySince = 0;
  if (!s_gPresent && near) {
    s_gPresent = true; s_gAwaySince = 0;
    runAction(S.dgBackM, "you're back");
    iotEvent("desk", "returned"); iotState("presence", true);
  }
}

// ---- shared
bool watchBusy() { return s_tArmed || s_gArmed; }
const char* watchStatusLine() {
  static char b[28];
  if (s_tArmed) snprintf(b, sizeof(b), s_tTripped ? "TRIPWIRE TRIPPED" : "tripwire armed");
  else if (s_gArmed) {
    static const char* N[] = { "", "guard: at desk", "guard: leaving", "guard: away", "guard: watching", "guard: someone!" };
    snprintf(b, sizeof(b), "%s", N[guardState()]);
  } else b[0] = 0;
  return b;
}
void watchLoop() {
  bool need = s_tArmed || s_gArmed || currentApp() == APP_TRIP || currentApp() == APP_GUARD;
  if (need) { uint16_t mm; s_ok = tofReadMm(mm); if (s_ok) s_mm = mm; }
  tripTick();
  // Any key silences a tripped siren (it stays armed and shows TRIPPED). Behind
  // a code lock the lock screen eats the keys first, so only you can silence it.
  if (s_tTripped && !s_tSilenced && !powerLocked()) {
    for (uint8_t k = 0; k < BTN_COUNT; k++) if (btnPressed(k)) { tripSilence(); toast("Siren off"); break; }
  }
  guardTick();
}

// ---- protocol
static bool cWatch(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  if (!q["trip"].isNull())  tripArm(q["trip"] | false);
  if (!q["guard"].isNull()) guardArm(q["guard"] | false);
  if (q["silence"] | false) tripSilence();
  r["trip"] = s_tArmed; r["tripped"] = s_tTripped; r["guard"] = s_gArmed;
  static const char* GN[] = { "off", "present", "leaving", "away", "watching", "someone" };
  r["guardState"] = GN[guardState()]; r["last"] = s_gLast;
  if (s_ok) r["mm"] = s_mm;
  r["tof"] = tofPresent();
  return true;
}
void watchBegin() { protoRegister("watch", cWatch); }
