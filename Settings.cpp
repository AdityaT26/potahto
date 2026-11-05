// Per-key NVS persistence that survives firmware updates
//
// Old scheme (fw <= 1.0): the whole Settings struct was one blob guarded by a
// version number, so any struct change (i.e. most firmware updates) wiped it.
//
// Current scheme: every field has its own NVS key, described once in FIELDS[].
//   load  = defaults, then overlay every key that exists in flash
//   save  = write each field, skipping ones whose stored value is unchanged
// Result: updates keep all known settings; new fields get their default;
// a field whose type changes is simply re-defaulted (type-checked on read).
//
// FIELDS[] also carries UI metadata (group, label, range, options, flags);
// the PC web app renders its entire Settings page from it.
//
// NVS key names are limited to 15 characters. Never REUSE a key for a field
// with a different meaning - pick a new key instead.
#include "Multitool.h"
#include "SettingsLegacy.h"
#include <stddef.h>

#define SETTINGS_NS      "mtl"       // shared with hi-scores / IR slots
#define KEY_SCHEMA       "schema"
#define KEY_OLD_BLOB     "cfg"       // legacy v1..v6 single-blob key (removed)

static bool     s_dirty = false;
static uint32_t s_dirtyAt = 0;

// The live struct must begin with the frozen v6 layout so migration can copy
// the common prefix byte-for-byte.
static_assert(offsetof(Settings, gAimInvV) == offsetof(SettingsV6, gAimInvV),
              "Settings must start with the exact SettingsV6 member layout");
static_assert(offsetof(Settings, battDivider) == offsetof(SettingsV6, battDivider),
              "Settings must start with the exact SettingsV6 member layout");

// ---- field table
//         key          type     field            group        label                 lo    hi   step  opts            flags
#define FX(k, t, f, g, l, lo, hi, st, o, fl) { k, t, (void*)&S.f, (uint16_t)sizeof(S.f), g, l, lo, hi, st, o, fl }
#define AXES "X|Y|Z"
#define ONOFF nullptr

static const SettingField FIELDS[] = {
  // ---- Sound & haptics
  FX("buzEn",    ST_BOOL, buzzerEnabled,   "Sound",   "Buzzer",                0, 1, 1, ONOFF, 0),
  FX("buzVol",   ST_U8,   buzzerVolume,    "Sound",   "Volume",                0, 10, 1, nullptr, 0),
  FX("motEn",    ST_BOOL, motorEnabled,    "Sound",   "Vibration",             0, 1, 1, ONOFF, 0),
  FX("motStr",   ST_U8,   motorStrength,   "Sound",   "Vibration power",       0, 10, 1, nullptr, 0),
  FX("ntfMode",  ST_U8,   ntfMode,         "Sound",   "Notifications",         0, 2, 1, "Silent|Vibrate|Sound", 0),
  FX("sSound",   ST_BOOL, startupSound,    "Sound",   "Boot sound",            0, 1, 1, ONOFF, 0),
  FX("songAlarm",ST_U8,   songAlarm,       "Sound",   "Alarm tone",            0, 33, 1, nullptr, SF_SONG),
  FX("songTimer",ST_U8,   songTimer,       "Sound",   "Timer tone",            0, 33, 1, nullptr, SF_SONG),
  FX("songNtf",  ST_U8,   songNotify,      "Sound",   "Message tone",          0, 33, 1, nullptr, SF_SONG),
  FX("songBoot", ST_U8,   songBoot,        "Sound",   "Boot tune",             0, 33, 1, nullptr, SF_SONG),
  FX("motDur",   ST_U8,   motorDurPct,     "Sound",   "Vibration length (%)",  20, 200, 10, nullptr, SF_ADV),
  FX("motDurAll",ST_BOOL, motorDurAll,     "Sound",   "Scale long buzzes",     0, 1, 1, ONOFF, SF_ADV),
  FX("buzRes",   ST_U16,  buzResHz,        "Sound",   "Loudest tone (Hz)",     1500, 5000, 100, nullptr, SF_ADV),
  // ---- Display
  FX("rot180",   ST_BOOL, rotate180,       "Display", "Flip 180",      0, 1, 1, ONOFF, 0),
  FX("autoRot",  ST_BOOL, autoRotate,      "Display", "Auto-rotate",           0, 1, 1, ONOFF, 0),
  FX("clock24",  ST_BOOL, clock24,         "Display", "24-hour clock",         0, 1, 1, ONOFF, 0),
  FX("sAnim",    ST_BOOL, startupAnim,     "Display", "Boot animation",        0, 1, 1, ONOFF, 0),
  FX("npxEn",    ST_BOOL, neopixelEnabled, "Display", "RGB LED",               0, 1, 1, ONOFF, 0),
  FX("npxBri",   ST_U8,   neopixelBright,  "Display", "LED brightness",        0, 255, 8, nullptr, 0),
  FX("npxOff",   ST_U16,  npxOffS,         "Display", "LED off after (s)",     0, 600, 10, nullptr, SF_ZOFF),
  FX("rotThr",   ST_U8,   autoRotateThresh,"Display", "Rotate tilt (deg)",10, 80, 5, nullptr, SF_ADV),
  // ---- Power & lock
  FX("scrOff",   ST_U16,  scrOffS,         "Power",   "Screen off after (s)",  0, 600, 5, nullptr, SF_ZOFF),
  FX("lockMode", ST_U8,   lockMode,        "Power",   "Lock screen",           0, 2, 1, "Off|Clock|Code", 0),
  FX("lockDelay",ST_U8,   lockDelayM,      "Power",   "Lock after (min)",      0, 60, 1, nullptr, 0),
  FX("lockCode", ST_STR,  lockCode,        "Power",   "Unlock code",           0, 0, 0, nullptr, SF_SECRET | SF_CUSTOM),
  FX("sleepAft", ST_U8,   sleepAfterM,     "Power",   "Deep sleep after (min)",0, 240, 5, nullptr, SF_ZOFF),
  FX("battDiv",  ST_F32,  battDivider,     "Device",   "Battery divider",       1.0f, 6.0f, 0.01f, nullptr, SF_ADV),
  FX("battTrim", ST_F32,  battTrim,        "Device",   "Battery trim",          0.8f, 1.2f, 0.01f, nullptr, SF_ADV),
  // ---- Calibration (persisted, edited on-device)
  FX("mpuCal",   ST_BOOL, mpuCalibrated,   "Calibration", "Gyro calibrated",   0, 1, 1, ONOFF, SF_HIDDEN),
  FX("gyroBias", ST_BLOB, gyroBias,        "Calibration", "Gyro bias",         0, 0, 0, nullptr, SF_HIDDEN),
  FX("accBias",  ST_BLOB, accelBias,       "Calibration", "Level offset",      0, 0, 0, nullptr, SF_HIDDEN),
  FX("compCal",  ST_BOOL, compassCalibrated,"Calibration","Compass calibrated",0, 1, 1, ONOFF, SF_HIDDEN),
  FX("magMin",   ST_BLOB, magMin,          "Calibration", "Mag min",           0, 0, 0, nullptr, SF_HIDDEN),
  FX("magMax",   ST_BLOB, magMax,          "Calibration", "Mag max",           0, 0, 0, nullptr, SF_HIDDEN),
  // ---- Connection (USB / Bluetooth)
  FX("hidRoute", ST_U8,   hidRoute,        "Connection", "Send keys via", 0, 2, 1, "Auto|USB|Bluetooth", 0),
  FX("bleMode",  ST_U8,   bleMode,         "Connection", "Bluetooth",          0, 2, 1, "Off|When needed|Always on", 0),
  // ---- Air Mouse
  FX("mSens",    ST_U8,   mouseSens,       "Air Mouse", "Sensitivity",         1, 20, 1, nullptr, 0),
  FX("mScroll",  ST_U8,   scrollSens,      "Air Mouse", "Scroll speed",        1, 10, 1, nullptr, 0),
  FX("mAccel",   ST_U8,   mouseAccel,      "Air Mouse", "Acceleration",        0, 10, 1, nullptr, 0),
  FX("mSmooth",  ST_U8,   mouseSmooth,     "Air Mouse", "Smoothness",          0, 10, 1, nullptr, 0),
  FX("mScreen",  ST_U8,   mouseScreen,     "Air Mouse", "Screen",              0, 3, 1, "720p|1080p|1440p|4K", 0),
  FX("mHoldMv",  ST_BOOL, mouseHoldMoves,  "Air Mouse", "DOWN button",         0, 1, 1, "Hold to freeze|Hold to move", 0),
  FX("mAxH",     ST_U8,   mouseAxisH,      "Air Mouse", "Horizontal axis",     0, 2, 1, AXES, SF_ADV),
  FX("mAxV",     ST_U8,   mouseAxisV,      "Air Mouse", "Vertical axis",       0, 2, 1, AXES, SF_ADV),
  FX("mAxS",     ST_U8,   mouseAxisS,      "Air Mouse", "Scroll axis",         0, 2, 1, AXES, SF_ADV),
  FX("mInvX",    ST_BOOL, mouseInvertX,    "Air Mouse", "Invert X",            0, 1, 1, ONOFF, SF_ADV),
  FX("mInvY",    ST_BOOL, mouseInvertY,    "Air Mouse", "Invert Y",            0, 1, 1, ONOFF, SF_ADV),
  FX("mTilt",    ST_BOOL, mouseTiltComp,   "Air Mouse", "Tilt compensation",   0, 1, 1, ONOFF, SF_ADV),
  // ---- Presenter / macro pad / shutter / IR
  FX("presAlert",ST_U8,   presAlertMin,    "Presenter", "Talk length (min)",   0, 180, 1, nullptr, SF_ZOFF),
  FX("presWarn", ST_U8,   presWarnM,       "Presenter", "Warn before end (min)", 0, 30, 1, nullptr, SF_ZOFF),
  FX("presPtr",  ST_U8,   presPtr,         "Presenter", "Pointer",             0, 2, 1, "PowerPoint laser|Google Slides laser|Mouse arrow", 0),
  FX("presNext", ST_U8,   presNextKey,     "Presenter", "Next-slide key",      0, 2, 1, "Right arrow|Page Down|Space", 0),
  FX("macroAuto",ST_BOOL, macroAuto,       "Macro Pad", "Follow the PC app",   0, 1, 1, ONOFF, 0),
  FX("macroProf",ST_U8,   macroProfile,    "Macro Pad", "Macro profile",       0, 15, 1, nullptr, SF_HIDDEN),
  FX("shutDelay",ST_U8,   shutterDelay,    "Shutter",   "Self-timer (s)",      0, 10, 1, nullptr, 0),
  FX("irRep",    ST_U8,   irRepeat,        "IR Remote", "Extra repeats",       0, 3, 1, nullptr, 0),
  // ---- Tripwire
  FX("trDist",   ST_U16,  tripDistMm,      "Tripwire", "Trip closer than (mm)",30, 2000, 10, nullptr, 0),
  FX("trBuz",    ST_BOOL, tripUseBuzzer,   "Tripwire", "Siren",                0, 1, 1, ONOFF, 0),
  FX("trVol",    ST_U8,   tripVolume,      "Tripwire", "Siren volume",         0, 10, 1, nullptr, 0),
  FX("trMot",    ST_BOOL, tripUseMotor,    "Tripwire", "Vibrate",              0, 1, 1, ONOFF, 0),
  FX("trLed",    ST_BOOL, tripUseLed,      "Tripwire", "Flash LED",            0, 1, 1, ONOFF, 0),
  FX("trAuto",   ST_BOOL, tripAutoReset,   "Tripwire", "After it clears",      0, 1, 1, "Keep alarming|Stop", 0),
  // ---- Desk Guard
  FX("dgMode",   ST_U8,   dgMode,          "Desk Guard", "Mode",               0, 1, 1, "Guard my seat|Watch behind me", 0),
  FX("dgDist",   ST_U8,   dgDistCm,        "Desk Guard", "Away beyond (cm)",   30, 250, 5, nullptr, 0),
  FX("dgDelay",  ST_U8,   dgDelayS,        "Desk Guard", "Away after (s)",     3, 120, 1, nullptr, 0),
  FX("dgNear",   ST_U8,   dgNearCm,        "Desk Guard", "Someone closer (cm)",20, 250, 5, nullptr, 0),
  FX("dgLeaveM", ST_STR,  dgLeaveM,        "Desk Guard", "When I leave",       0, 0, 0, nullptr, SF_MACRO),
  FX("dgBackM",  ST_STR,  dgBackM,         "Desk Guard", "When I'm back",      0, 0, 0, nullptr, SF_MACRO),
  FX("dgNearM",  ST_STR,  dgNearM,         "Desk Guard", "When someone comes", 0, 0, 0, nullptr, SF_MACRO),
  FX("dgAction", ST_U8,   dgAction,        "Desk Guard", "legacy action",      0, 1, 1, nullptr, SF_HIDDEN),
  FX("dgWake",   ST_BOOL, dgWake,          "Desk Guard", "legacy wake",        0, 1, 1, ONOFF, SF_HIDDEN),
  // ---- Pomodoro
  FX("pomWork",  ST_U8,   pomWork,         "Pomodoro", "Focus (min)",          1, 90, 1, nullptr, 0),
  FX("pomShort", ST_U8,   pomShort,        "Pomodoro", "Short break (min)",    1, 30, 1, nullptr, 0),
  FX("pomLong",  ST_U8,   pomLong,         "Pomodoro", "Long break (min)",     1, 60, 1, nullptr, 0),
  FX("pomRounds",ST_U8,   pomRounds,       "Pomodoro", "Rounds per set",       1, 8, 1, nullptr, 0),
  FX("pomAuto",  ST_BOOL, pomAuto,         "Pomodoro", "Auto-start next",      0, 1, 1, ONOFF, 0),
  // ---- Compass / light / scale
  FX("magDecl",  ST_F32,  magDeclination,  "Compass", "Declination (deg)",-30, 30, 1, nullptr, 0),
  FX("magOff",   ST_F32,  magOffset,       "Compass", "North trim (deg)", -180, 180, 1, nullptr, SF_ADV),
  FX("liHue",    ST_U8,   lightHue,        "Light",   "Hue",                   0, 255, 1, nullptr, 0),
  FX("liBri",    ST_U8,   lightBright,     "Light",   "Brightness",            0, 255, 5, nullptr, 0),
  FX("liMode",   ST_U8,   lightMode,       "Light",   "Mode",                  0, LM_COUNT - 1, 1, "Solid|Breathe|Rainbow|Tilt hue", 0),
  FX("torchOff", ST_U8,   torchOffM,       "Light",   "Torch off after (min)", 0, 120, 1, nullptr, SF_ZOFF),
  FX("scUnit",   ST_U8,   scaleUnit,       "Scale",   "Unit",                  0, UNIT_COUNT - 1, 1, "mm|cm|in", 0),
  FX("scTare",   ST_I16,  scaleTareMm,     "Scale",   "Tare offset (mm)",      -2000, 2000, 1, nullptr, SF_ADV),
  // ---- Games
  FX("gMus",     ST_BOOL, gameMusic,       "Games",   "Music",                 0, 1, 1, ONOFF, 0),
  FX("gSfx",     ST_BOOL, gameSfx,         "Games",   "Sound effects",         0, 1, 1, ONOFF, 0),
  FX("gHap",     ST_BOOL, gameHaptics,     "Games",   "Haptics",               0, 1, 1, ONOFF, 0),
  FX("gNpx",     ST_BOOL, gameNeopixel,    "Games",   "LED effects",           0, 1, 1, ONOFF, 0),
  FX("gTiltAxH", ST_U8,   gTiltAxisH,      "Games",   "Tilt horizontal axis",  0, 2, 1, AXES, SF_ADV),
  FX("gTiltInH", ST_BOOL, gTiltInvH,       "Games",   "Tilt invert horizontal",0, 1, 1, ONOFF, SF_ADV),
  FX("gTiltAxV", ST_U8,   gTiltAxisV,      "Games",   "Tilt vertical axis",    0, 2, 1, AXES, SF_ADV),
  FX("gTiltInV", ST_BOOL, gTiltInvV,       "Games",   "Tilt invert vertical",  0, 1, 1, ONOFF, SF_ADV),
  FX("gAimAxH",  ST_U8,   gAimAxisH,       "Games",   "Aim horizontal axis",   0, 2, 1, AXES, SF_ADV),
  FX("gAimInH",  ST_BOOL, gAimInvH,        "Games",   "Aim invert horizontal", 0, 1, 1, ONOFF, SF_ADV),
  FX("gAimAxV",  ST_U8,   gAimAxisV,       "Games",   "Aim vertical axis",     0, 2, 1, AXES, SF_ADV),
  FX("gAimInV",  ST_BOOL, gAimInvV,        "Games",   "Aim invert vertical",   0, 1, 1, ONOFF, SF_ADV),
  // ---- Device
  FX("devName",  ST_STR,  devName,         "Device",  "Device name",           0, 0, 0, nullptr, SF_REBOOT),
  FX("webPin",   ST_STR,  webPin,          "Device",  "Web PIN",               0, 0, 0, nullptr, SF_SECRET),
  FX("tz",       ST_STR,  tz,              "Device",  "Time zone (POSIX)",     0, 0, 0, nullptr, SF_ADV),
  // ---- Alarms (Desk page)
  FX("al0On",    ST_BOOL, alarmOn[0],      "Alarms", "Alarm 1",                0, 1, 1, ONOFF, SF_CUSTOM),
  FX("al0H",     ST_U8,   alarmH[0],       "Alarms", "Alarm 1 hour",           0, 23, 1, nullptr, SF_CUSTOM),
  FX("al0M",     ST_U8,   alarmM[0],       "Alarms", "Alarm 1 minute",         0, 59, 1, nullptr, SF_CUSTOM),
  FX("al0D",     ST_U8,   alarmDays[0],    "Alarms", "Alarm 1 days",           0, 127, 1, nullptr, SF_CUSTOM),
  FX("al1On",    ST_BOOL, alarmOn[1],      "Alarms", "Alarm 2",                0, 1, 1, ONOFF, SF_CUSTOM),
  FX("al1H",     ST_U8,   alarmH[1],       "Alarms", "Alarm 2 hour",           0, 23, 1, nullptr, SF_CUSTOM),
  FX("al1M",     ST_U8,   alarmM[1],       "Alarms", "Alarm 2 minute",         0, 59, 1, nullptr, SF_CUSTOM),
  FX("al1D",     ST_U8,   alarmDays[1],    "Alarms", "Alarm 2 days",           0, 127, 1, nullptr, SF_CUSTOM),
  FX("al2On",    ST_BOOL, alarmOn[2],      "Alarms", "Alarm 3",                0, 1, 1, ONOFF, SF_CUSTOM),
  FX("al2H",     ST_U8,   alarmH[2],       "Alarms", "Alarm 3 hour",           0, 23, 1, nullptr, SF_CUSTOM),
  FX("al2M",     ST_U8,   alarmM[2],       "Alarms", "Alarm 3 minute",         0, 59, 1, nullptr, SF_CUSTOM),
  FX("al2D",     ST_U8,   alarmDays[2],    "Alarms", "Alarm 3 days",           0, 127, 1, nullptr, SF_CUSTOM),
  FX("timerMin", ST_U8,   timerMin,        "Alarms", "Timer (min)",            1, 180, 1, nullptr, SF_CUSTOM),
  // ---- WiFi (networks are edited on the WiFi page / device WiFi menu)
  FX("wifiMode", ST_U8,   wifiMode,        "WiFi",    "WiFi",                  0, WM_COUNT - 1, 1, "Off|On demand|Always on", 0),
#define WSLOT(n) \
  FX("wSsid" #n, ST_STR, wifiSsid[n],      "WiFi",    "Network " #n,           0, 0, 0, nullptr, SF_CUSTOM), \
  FX("wPass" #n, ST_STR, wifiPass[n],      "WiFi",    "Password " #n,          0, 0, 0, nullptr, SF_SECRET | SF_CUSTOM), \
  FX("wUser" #n, ST_STR, wifiUser[n],      "WiFi",    "Username " #n,          0, 0, 0, nullptr, SF_CUSTOM), \
  FX("wEap" #n,  ST_U8,  wifiEap[n],       "WiFi",    "Security " #n,          0, 2, 1, "Personal|Enterprise PEAP|Enterprise TTLS", SF_CUSTOM)
  WSLOT(0), WSLOT(1), WSLOT(2), WSLOT(3), WSLOT(4),
#undef WSLOT
};
static const size_t NFIELDS = sizeof(FIELDS) / sizeof(FIELDS[0]);

const SettingField* settingsFields(size_t* count) { if (count) *count = NFIELDS; return FIELDS; }
const SettingField* settingsFind(const char* key) {
  if (!key) return nullptr;
  for (size_t i = 0; i < NFIELDS; i++) if (strcmp(FIELDS[i].key, key) == 0) return &FIELDS[i];
  return nullptr;
}

// ---- typed access (used by the config protocol)
static int optCount(const char* o) {
  if (!o) return 0;
  int n = 1; for (; *o; o++) if (*o == '|') n++;
  return n;
}
double settingsGetAsDouble(const SettingField& f) {
  switch (f.type) {
    case ST_BOOL: return *(bool*)f.ptr ? 1 : 0;
    case ST_U8:   return *(uint8_t*)f.ptr;
    case ST_U16:  return *(uint16_t*)f.ptr;
    case ST_I16:  return *(int16_t*)f.ptr;
    case ST_F32:  return *(float*)f.ptr;
    default:      return 0;
  }
}
bool settingsSetFromDouble(const SettingField& f, double v) {
  if (isnan(v) || isinf(v)) return false;
  if (f.type == ST_BOOL) { *(bool*)f.ptr = (v != 0); return true; }
  if (f.type == ST_STR || f.type == ST_BLOB) return false;
  double lo = f.lo, hi = f.hi;
  if (f.opts) { lo = 0; hi = optCount(f.opts) - 1; }
  if (hi > lo) v = v < lo ? lo : (v > hi ? hi : v);
  switch (f.type) {
    case ST_U8:  *(uint8_t*)f.ptr  = (uint8_t)lround(v);  break;
    case ST_U16: *(uint16_t*)f.ptr = (uint16_t)lround(v); break;
    case ST_I16: *(int16_t*)f.ptr  = (int16_t)lround(v);  break;
    case ST_F32: *(float*)f.ptr    = (float)v;            break;
    default: return false;
  }
  return true;
}
bool settingsSetFromString(const SettingField& f, const char* s) {
  if (!s) return false;
  if (f.type == ST_STR) {
    size_t n = strlen(s);
    if (n >= f.size) return false;                    // too long: reject, don't truncate
    for (size_t i = 0; i < n; i++) if ((uint8_t)s[i] < 0x20) return false;   // no control chars
    memcpy(f.ptr, s, n + 1);
    return true;
  }
  if (f.type == ST_BLOB) return false;
  if (f.type == ST_BOOL && (!strcasecmp(s, "true") || !strcasecmp(s, "on")))  { *(bool*)f.ptr = true;  return true; }
  if (f.type == ST_BOOL && (!strcasecmp(s, "false") || !strcasecmp(s, "off"))) { *(bool*)f.ptr = false; return true; }
  char* end = nullptr;
  double v = strtod(s, &end);
  if (end == s) return false;
  return settingsSetFromDouble(f, v);
}

// ---- per-field NVS I/O
// Scalars use native NVS integer types (1 NVS entry each). Floats are stored as
// their raw 32-bit pattern in a u32 (Preferences::putFloat would use a blob,
// which costs ~3 entries). Arrays are blobs, strings are NVS strings.
static void readField(const SettingField& f) {
  if (!prefs.isKey(f.key)) return;                      // keep default
  switch (f.type) {
    case ST_BOOL: *(bool*)f.ptr     = prefs.getUChar(f.key, *(bool*)f.ptr) != 0; break;
    case ST_U8:   *(uint8_t*)f.ptr  = prefs.getUChar(f.key, *(uint8_t*)f.ptr);   break;
    case ST_U16:  *(uint16_t*)f.ptr = prefs.getUShort(f.key, *(uint16_t*)f.ptr); break;
    case ST_I16:  *(int16_t*)f.ptr  = prefs.getShort(f.key, *(int16_t*)f.ptr);   break;
    case ST_F32: {
      uint32_t bits; memcpy(&bits, f.ptr, 4);
      bits = prefs.getUInt(f.key, bits);
      float v; memcpy(&v, &bits, 4);
      if (!isnan(v) && !isinf(v)) memcpy(f.ptr, &v, 4);
    } break;
    case ST_BLOB:
      if (prefs.getBytesLength(f.key) == f.size) prefs.getBytes(f.key, f.ptr, f.size);
      break;
    case ST_STR: {
      char tmp[128];
      size_t n = prefs.getString(f.key, tmp, sizeof(tmp));
      if (n > 0 && strlen(tmp) < f.size) memcpy(f.ptr, tmp, strlen(tmp) + 1);
    } break;
  }
}
static void writeField(const SettingField& f) {
  bool have = prefs.isKey(f.key);
  switch (f.type) {
    case ST_BOOL: {
      uint8_t v = *(bool*)f.ptr ? 1 : 0;
      if (!have || prefs.getUChar(f.key, v ^ 1) != v) prefs.putUChar(f.key, v);
    } break;
    case ST_U8: {
      uint8_t v = *(uint8_t*)f.ptr;
      if (!have || prefs.getUChar(f.key, v ^ 1) != v) prefs.putUChar(f.key, v);
    } break;
    case ST_U16: {
      uint16_t v = *(uint16_t*)f.ptr;
      if (!have || prefs.getUShort(f.key, v ^ 1) != v) prefs.putUShort(f.key, v);
    } break;
    case ST_I16: {
      int16_t v = *(int16_t*)f.ptr;
      if (!have || prefs.getShort(f.key, v ^ 1) != v) prefs.putShort(f.key, v);
    } break;
    case ST_F32: {
      uint32_t v; memcpy(&v, f.ptr, 4);
      if (!have || prefs.getUInt(f.key, v ^ 1) != v) prefs.putUInt(f.key, v);
    } break;
    case ST_BLOB: {
      uint8_t cur[32];
      bool same = have && f.size <= sizeof(cur) &&
                  prefs.getBytesLength(f.key) == f.size &&
                  prefs.getBytes(f.key, cur, f.size) == f.size &&
                  memcmp(cur, f.ptr, f.size) == 0;
      if (!same) prefs.putBytes(f.key, f.ptr, f.size);
    } break;
    case ST_STR: {
      char cur[128] = {0};
      bool same = have && prefs.getString(f.key, cur, sizeof(cur)) > 0 &&
                  strcmp(cur, (const char*)f.ptr) == 0;
      if (!same) prefs.putString(f.key, (const char*)f.ptr);
    } break;
  }
}

// ---- defaults
void settingsLoadDefaults() {
  memset(&S, 0, sizeof(S));
  S.magic   = SETTINGS_MAGIC;
  S.version = SETTINGS_VERSION;

  S.buzzerEnabled   = true;
  S.motorEnabled    = true;
  S.neopixelEnabled = true;
  S.buzzerVolume    = 7;
  S.motorStrength   = 7;
  S.neopixelBright  = 140;
  S.motorDurPct     = 100;
  S.motorDurAll     = false;

  S.startupAnim  = true;
  S.startupSound = true;

  S.rotate180        = false;
  S.autoRotate       = false;
  S.autoRotateThresh = 45;

  S.battDivider = BATT_DIVIDER_DEFAULT;
  S.battTrim    = 1.0f;

  S.mpuCalibrated = false;
  for (int i = 0; i < 3; i++) { S.gyroBias[i] = 0; S.accelBias[i] = 0; }

  S.compassCalibrated = false;
  for (int i = 0; i < 3; i++) { S.magMin[i] = 32767; S.magMax[i] = -32768; }
  S.magDeclination = 0.0f;
  S.magOffset = 0.0f;

  // ---- air mouse: the standard MPU orientation for this build
  // Horiz = gyro Z, Vert = gyro Y, Scroll = gyro X, Invert X on, Invert Y off,
  // Tilt comp off, DOWN = hold-to-move. The games' gyro aim uses the same axes.
  S.mouseSens    = 8;
  S.scrollSens   = 3;         // new finer scale: 1 is ~4x slower than old "1"
  S.mouseInvertX = true;
  S.mouseInvertY = false;
  S.mouseAxisH   = 2;         // gyro Z -> left/right
  S.mouseAxisV   = 1;         // gyro Y -> up/down
  S.mouseAxisS   = 0;         // gyro X -> scroll wheel
  S.mouseTiltComp= false;
  S.mouseAccel   = 4;
  S.mouseSmooth  = 3;
  S.mouseScreen  = 3;         // 4K (change in Settings to match your monitor)
  S.mouseHoldMoves = true;    // DOWN = move (hold to move)

  S.tripDistMm   = 300;
  S.tripUseMotor = true;
  S.tripUseBuzzer= true;
  S.tripUseLed   = true;
  S.tripVolume   = 8;
  S.tripAutoReset= true;

  S.lightHue    = 0;
  S.lightBright = 160;
  S.lightMode   = LM_SOLID;

  S.scaleUnit   = UNIT_MM;
  S.scaleTareMm = 0;

  S.gameMusic    = true;
  S.gameSfx      = true;
  S.gameHaptics  = true;
  S.gameNeopixel = true;
  S.gTiltAxisH = 0; S.gTiltInvH = false;   // accel X -> horizontal
  S.gTiltAxisV = 1; S.gTiltInvV = false;   // accel Y -> vertical
  S.gAimAxisH  = 2; S.gAimInvH  = false;   // gyro Z  -> horizontal (= air mouse H)
  S.gAimAxisV  = 1; S.gAimInvV  = false;   // gyro Y  -> vertical   (= air mouse V)

  // ---- device / connectivity
  strcpy(S.devName, "Potahto");
  S.webPin[0] = 0;                         // generated on first boot
  S.wifiMode  = WM_ON_DEMAND;
  strcpy(S.tz, "IST-5:30");                // India Standard Time (UTC+5:30)

  S.hidRoute     = HR_AUTO;
  S.bleMode      = BM_AT_BOOT;
  S.presNextKey  = 0;
  S.presAlertMin = 0;
  S.shutterDelay = 3;
  S.macroProfile = 0;

  S.pomWork = 25; S.pomShort = 5; S.pomLong = 15; S.pomRounds = 4; S.pomAuto = false;
  S.dgDistCm = 90; S.dgDelayS = 15; S.dgAction = 1; S.dgWake = true;
  for (int i = 0; i < ALARMS; i++) { S.alarmOn[i] = false; S.alarmH[i] = 7; S.alarmM[i] = 0; S.alarmDays[i] = 0x1F; }
  S.timerMin = 10;
  S.ntfMode = 2;

  S.scrOffS = 30; S.lockMode = 1; S.lockDelayM = 2; S.lockCode[0] = 0;
  S.sleepAfterM = 30; S.npxOffS = 60; S.torchOffM = 10; S.clock24 = true;
  S.buzResHz = 2700; S.songAlarm = S.songTimer = S.songNotify = S.songBoot = 0;
  S.presWarnM = 2; S.presPtr = 0; S.macroAuto = true; S.irRepeat = 1;
  S.dgMode = 0; S.dgNearCm = 60;
  strcpy(S.dgLeaveM, "{win+l}"); strcpy(S.dgBackM, "{shift}"); strcpy(S.dgNearM, "{alt+tab}");
}

// ---- lifecycle
static void writeAll() {
  for (size_t i = 0; i < NFIELDS; i++) writeField(FIELDS[i]);
  if (prefs.getUShort(KEY_SCHEMA, 0) != SETTINGS_VERSION) prefs.putUShort(KEY_SCHEMA, SETTINGS_VERSION);
}

// One-time migration from the legacy single-blob format (schema 6). The live
// struct starts with the frozen v6 layout, so the common prefix is copied
// directly. Calibration and general settings are kept; the air-mouse / aim
// orientation and scroll speed are forced to the new standard defaults
// (scroll also uses a new, finer scale).
static bool migrateLegacyBlob() {
  if (!prefs.isKey(KEY_OLD_BLOB)) return false;
  if (prefs.getBytesLength(KEY_OLD_BLOB) != sizeof(SettingsV6)) return false;
  SettingsV6 old;
  prefs.getBytes(KEY_OLD_BLOB, &old, sizeof(old));
  if (old.magic != SETTINGS_MAGIC || old.version != 6) return false;
  Settings def = S;                    // S currently holds defaults
  memcpy(&S, &old, offsetof(SettingsV6, gAimInvV) + sizeof(old.gAimInvV));
  S.magic = SETTINGS_MAGIC; S.version = SETTINGS_VERSION;
  S.mouseAxisH = def.mouseAxisH;  S.mouseAxisV = def.mouseAxisV;  S.mouseAxisS = def.mouseAxisS;
  S.mouseInvertX = def.mouseInvertX;  S.mouseInvertY = def.mouseInvertY;
  S.mouseTiltComp = def.mouseTiltComp; S.mouseHoldMoves = def.mouseHoldMoves;
  S.scrollSens = def.scrollSens;
  S.gAimAxisH = def.gAimAxisH;  S.gAimAxisV = def.gAimAxisV;
  S.gAimInvH  = def.gAimInvH;   S.gAimInvV  = def.gAimInvV;
  return true;
}

static void ensurePin() {
  bool ok = strlen(S.webPin) == 6;
  for (int i = 0; ok && i < 6; i++) ok = isdigit((unsigned char)S.webPin[i]);
  if (!ok) snprintf(S.webPin, sizeof(S.webPin), "%06lu", (unsigned long)(esp_random() % 1000000UL));
}

void settingsBegin() {
  prefs.begin(SETTINGS_NS, false);             // read/write, persists app-wide
  settingsLoadDefaults();
  if (!prefs.isKey(KEY_SCHEMA)) migrateLegacyBlob();   // first boot on per-key fw
  for (size_t i = 0; i < NFIELDS; i++) readField(FIELDS[i]);
  if (prefs.isKey(KEY_OLD_BLOB)) prefs.remove(KEY_OLD_BLOB);   // reclaim legacy blob
  ensurePin();
  if (!S.devName[0]) strcpy(S.devName, "Potahto");
  if (!prefs.isKey("dgLeaveM") && prefs.isKey("dgAction")) {   // 1.2 -> 1.3: actions became macros
    strcpy(S.dgLeaveM, S.dgAction == 1 ? "{win+l}" : "");
    strcpy(S.dgBackM, S.dgWake ? "{shift}" : "");
  }
  writeAll();                                  // persist defaults for any new keys
}

static uint32_t s_savedAt = 0;
void settingsSave() {
  S.magic   = SETTINGS_MAGIC;
  S.version = SETTINGS_VERSION;
  writeAll();
  s_dirty = false;
  s_savedAt = millis();
}
uint32_t settingsSavedAgoMs() { return s_savedAt ? millis() - s_savedAt : 0xFFFFFFFF; }
bool settingsPending() { return s_dirty; }

void settingsSaveSoon() { s_dirty = true; s_dirtyAt = millis(); }

void settingsTick() {
  if (s_dirty && (millis() - s_dirtyAt) > 1200) settingsSave();
}

// Factory reset restores settings + calibration defaults. Hi-scores, saved IR
// codes and the web PIN are intentionally kept (the PIN so an already-paired
// browser keeps working; change it explicitly from the web app if needed).
void settingsFactoryReset() {
  char pin[sizeof(S.webPin)]; memcpy(pin, S.webPin, sizeof(pin));
  settingsLoadDefaults();
  memcpy(S.webPin, pin, sizeof(pin));
  ensurePin();
  settingsSave();
}
