// Persistent configuration (ESP32 NVS / Preferences)
//
// Every field is stored under its own NVS key (see Settings.cpp). Loading
// starts from defaults and then overlays whatever keys exist, so:
//   * flashing new firmware keeps every setting the old firmware knew about
//   * a newly added field simply takes its default on existing devices
//   * removing / reordering struct fields never corrupts anything
// To add a setting: add the field here, give it a default in
// settingsLoadDefaults(), and add one line to the FIELDS table in Settings.cpp.
#pragma once
#include <Arduino.h>

#define SETTINGS_MAGIC    0x4D544C31UL   // "MTL1"
#define SETTINGS_VERSION  9      // schema number (informational; no wipe on change)

enum ScaleUnit : uint8_t { UNIT_MM = 0, UNIT_CM, UNIT_INCH, UNIT_COUNT };
enum LightMode : uint8_t { LM_SOLID = 0, LM_BREATHE, LM_RAINBOW, LM_GYRO, LM_COUNT };
enum WifiMode  : uint8_t { WM_OFF = 0, WM_ON_DEMAND, WM_ALWAYS, WM_COUNT };
#define WIFI_SLOTS 5            // saved WiFi networks
#define ALARMS     3            // clock alarms

struct Settings {
  uint32_t magic;
  uint16_t version;

  // ---- feedback master switches + strengths
  bool     buzzerEnabled;
  bool     motorEnabled;
  bool     neopixelEnabled;
  uint8_t  buzzerVolume;      // 0..10
  uint8_t  motorStrength;     // 0..10
  uint8_t  neopixelBright;    // 0..255  (global brightness cap)
  uint8_t  motorDurPct;       // 20..200 % scale of motor pulse durations
  bool     motorDurAll;       // also scale the long/special pulses

  // ---- startup
  bool     startupAnim;
  bool     startupSound;

  // ---- display
  bool     rotate180;
  bool     autoRotate;
  uint8_t  autoRotateThresh;  // tilt degrees needed to flip (10..80)

  // ---- battery
  float    battDivider;
  float    battTrim;          // fine multiplicative trim (~1.0)

  // ---- MPU6050 calibration
  bool     mpuCalibrated;
  float    gyroBias[3];       // deg/s
  float    accelBias[3];      // m/s^2, levels the bubble

  // ---- compass calibration
  bool     compassCalibrated;
  int16_t  magMin[3];
  int16_t  magMax[3];
  float    magDeclination;    // degrees, +E
  float    magOffset;         // manual heading offset (aligns sensor to screen)

  // ---- air-mouse
  uint8_t  mouseSens;         // 1..20
  uint8_t  scrollSens;        // 1..10
  bool     mouseInvertX;
  bool     mouseInvertY;
  uint8_t  mouseAxisH;        // gyro axis driving horizontal: 0=X 1=Y 2=Z
  uint8_t  mouseAxisV;        // gyro axis driving vertical:   0=X 1=Y 2=Z
  uint8_t  mouseAxisS;        // gyro axis driving scroll wheel: 0=X 1=Y 2=Z
  bool     mouseTiltComp;     // rotate motion by held-roll (horizon lock)
  uint8_t  mouseAccel;        // 0..10 pointer acceleration (nonlinear gain)
  uint8_t  mouseSmooth;       // 0..10 motion smoothing (low-pass)
  uint8_t  mouseScreen;       // 0=720p 1=1080p 2=1440p 3=4K (scales gain)
  bool     mouseHoldMoves;    // DOWN: false=hold-to-freeze, true=hold-to-move

  // ---- tripwire
  uint16_t tripDistMm;        // trigger closer than this
  bool     tripUseMotor;
  bool     tripUseBuzzer;
  bool     tripUseLed;
  uint8_t  tripVolume;        // 0..10 (local buzzer volume)
  bool     tripAutoReset;     // true: clears when object leaves; false: latches

  // ---- light tool
  uint8_t  lightHue;          // 0..255
  uint8_t  lightBright;       // 0..255
  uint8_t  lightMode;         // LightMode

  // ---- scale
  uint8_t  scaleUnit;         // ScaleUnit
  int16_t  scaleTareMm;

  // ---- games
  bool     gameMusic;         // chiptune background track (during games only)
  bool     gameSfx;           // game sound effects
  bool     gameHaptics;       // game vibration
  bool     gameNeopixel;      // game LED effects
  // motion mapping for sensor games (which axis drives which game motion)
  uint8_t  gTiltAxisH; bool gTiltInvH;   // accel -> horizontal
  uint8_t  gTiltAxisV; bool gTiltInvV;   // accel -> vertical
  uint8_t  gAimAxisH;  bool gAimInvH;    // gyro  -> horizontal
  uint8_t  gAimAxisV;  bool gAimInvV;    // gyro  -> vertical
  // ^^^^ everything above is the frozen v6 prefix (SettingsLegacy.h) ^^^^
  //      add NEW fields only below this line.

  // ---- device / connectivity (fw 1.2)
  char     devName[24];         // hostname (potahto.local), BLE + AP name
  char     webPin[8];           // 6-digit PIN for the HTTP API (generated)
  uint8_t  wifiMode;            // WifiMode
  char     wifiSsid[WIFI_SLOTS][33];
  char     wifiPass[WIFI_SLOTS][65];
  char     wifiUser[WIFI_SLOTS][65];   // enterprise (campus) username; empty = normal WPA2
  uint8_t  wifiEap[WIFI_SLOTS];        // 0 personal, 1 PEAP, 2 TTLS
  char     tz[40];              // POSIX TZ string, e.g. "IST-5:30"

  // ---- control / HID (fw 1.2)
  uint8_t  hidRoute;            // HidRoute: Auto / USB / Bluetooth
  uint8_t  bleMode;             // BleMode: Off / when needed / at boot
  uint8_t  presNextKey;         // presenter "next": 0 Right, 1 Page Down, 2 Space
  uint8_t  presAlertMin;        // presenter: buzz after N minutes (0 = off)
  uint8_t  shutterDelay;        // camera shutter self-timer, seconds
  uint8_t  macroProfile;        // last macro-pad profile

  // ---- desk (fw 1.2)
  uint8_t  pomWork, pomShort, pomLong, pomRounds;   // minutes, minutes, minutes, count
  bool     pomAuto;             // start the next phase automatically
  uint8_t  dgDistCm;            // Desk Guard: "away" beyond this distance
  uint8_t  dgDelayS;            // ... for this long
  uint8_t  dgAction;            // 0 nothing, 1 lock PC (Win+L)
  bool     dgWake;              // tap a key when you come back
  bool     alarmOn[ALARMS];
  uint8_t  alarmH[ALARMS], alarmM[ALARMS];
  uint8_t  alarmDays[ALARMS];   // bit0 = Monday ... bit6 = Sunday; 0 = once
  uint8_t  timerMin;            // last countdown-timer length
  uint8_t  ntfMode;             // notifications: 0 silent, 1 vibrate, 2 sound + vibrate

  // ---- power / display (fw 1.3)
  uint16_t scrOffS;             // screen off after N s idle (0 = never)
  uint8_t  lockMode;            // 0 off, 1 clock face (any key), 2 code
  uint8_t  lockDelayM;          // lock only after the screen was off this long (min)
  char     lockCode[8];         // unlock code: letters U D L R (4..6)
  uint8_t  sleepAfterM;         // deep sleep after the screen was off N min (0 = never)
  uint16_t npxOffS;             // RGB LED off after N s idle (0 = never)
  uint8_t  torchOffM;           // torch auto-off (min, 0 = never)
  bool     clock24;             // 24-hour clock
  // ---- sound (fw 1.3)
  uint16_t buzResHz;            // loudest frequency of this buzzer (alarms centre on it)
  uint8_t  songAlarm, songTimer, songNotify, songBoot;   // 0 = built-in, n = song slot n
  // ---- control (fw 1.3)
  uint8_t  presWarnM;           // presenter: warn this many minutes before the end
  uint8_t  presPtr;             // presenter pointer: 0 PowerPoint laser, 1 Slides laser, 2 mouse
  bool     macroAuto;           // macro pad follows the PC's active app (companion)
  uint8_t  irRepeat;            // extra IR repeats for range
  // ---- desk guard (fw 1.3)
  uint8_t  dgMode;              // 0 seat (you leave / return), 1 watch (someone comes close)
  uint8_t  dgNearCm;            // watch mode: trigger closer than this
  char     dgLeaveM[100];       // macro when you leave   (seat mode)
  char     dgBackM[100];        // macro when you return  (seat mode)
  char     dgNearM[100];        // macro when someone comes close (watch mode)
};

// Global instance (defined in Globals.cpp)
extern Settings S;

// ---- field table (drives NVS persistence and the PC config protocol)
// One entry per setting. The web app builds its whole Settings UI from this
// table (group, label, range, options), so a new setting shows up on the PC
// automatically.
enum SettingType : uint8_t { ST_BOOL = 0, ST_U8, ST_U16, ST_I16, ST_F32, ST_BLOB, ST_STR };
enum SettingFlag : uint8_t {
  SF_HIDDEN = 0x01,    // internal (calibration blobs): persisted, not shown
  SF_SECRET = 0x02,    // write-only over the network (passwords, PIN)
  SF_REBOOT = 0x04,    // takes effect after a restart
  SF_ADV    = 0x08,    // "advanced" - collapsed by default in the web app
  SF_CUSTOM = 0x10,    // edited on a dedicated web-app page, not the Settings page
  SF_ZOFF   = 0x20,    // value 0 means "off/never" (shown as such)
  SF_SONG   = 0x40,    // value is a song slot (0 = built-in tone)
  SF_MACRO  = 0x80,    // string is a macro (validated, edited with the macro editor)
};
struct SettingField {
  const char* key;     // NVS key (<= 15 chars), also the config-protocol name
  SettingType type;
  void*       ptr;     // into S
  uint16_t    size;    // sizeof the field (strings: buffer incl. NUL)
  const char* group;   // UI section
  const char* label;   // UI label
  float       lo, hi, step;   // numeric range (ignored for bool/str/blob)
  const char* opts;    // "A|B|C" -> enum (value = index); bool: "off|on" labels
  uint8_t     flags;   // SettingFlag bits
};
const SettingField* settingsFields(size_t* count);
const SettingField* settingsFind(const char* key);
// Set a field from a string/number with type checking + clamping to [lo,hi].
// Returns false (and leaves the value untouched) if the input is invalid.
bool settingsSetFromDouble(const SettingField& f, double v);
bool settingsSetFromString(const SettingField& f, const char* s);
double settingsGetAsDouble(const SettingField& f);

void settingsBegin();          // load or initialise
void settingsSave();           // write immediately
void settingsSaveSoon();       // mark dirty; flushed by settingsTick()
void settingsTick();           // debounced flush, call each loop
uint32_t settingsSavedAgoMs(); // for the "saved" tick in Settings
bool settingsPending();        // a change is waiting to be written
void settingsLoadDefaults();   // reset RAM copy to defaults (not saved)
void settingsFactoryReset();   // defaults + save + clears calibration
