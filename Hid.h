// Keyboard + mouse + media keys over USB and Bluetooth LE
//
// One API, two transports:
//   USB : TinyUSB composite (CDC serial + keyboard + mouse + consumer).
//         Needs "USB Mode: USB-OTG (TinyUSB)" (the XIAO_ESP32S3 default).
//   BLE : Potahto's own NimBLE composite HID - one pairing gives the host a
//         keyboard, a mouse and media keys (replaces ESP32-NimBLE-Mouse).
// Routing (Settings > Bluetooth > "Keys & mouse go to"): Auto = whichever
// connected last (plug in the cable -> USB, a PC connects over Bluetooth ->
// Bluetooth); or force USB / Bluetooth. Air Mouse: hold UP to switch.
//
// Macros (macro pad, web app "Test"): plain text is typed (US layout), and
// {...} tokens do everything else:
//   {enter} {tab} {esc} {f5} {up} {pgdn} ...   single keys
//   {ctrl+shift+t} {win+r} {alt+f4}            chords (ctrl shift alt win/cmd/gui,
//                                              r-prefixed for right-hand mods)
//   {delay 300}                                pause (ms)
//   {media playpause|next|prev|stop|mute|volup|voldown|brightup|brightdown|calc|browser|mail|search}
//   {mouse left|right|middle}  {scroll -3}
//   {pointer} {pointer click} {pointer drag}   hand over to the gyro pointer
//                                              (e.g. {win+shift+s}{pointer drag})
//   {run notepad}                              Win+R, type, Enter
//   {cmd ipconfig /flushdns}                   a hidden-ish cmd window runs it
//   {admin netsh wlan show profiles}           same, as administrator (UAC: Alt+Y)
//   {pc shutdown /h}                           the PC companion runs it (silent;
//                                              companion started with --allow-run)
//   {{                                          a literal "{"
// Playback is queued and non-blocking (hidLoop()).
#pragma once
#include <Arduino.h>

enum HidRoute : uint8_t { HR_AUTO = 0, HR_USB, HR_BLE, HR_COUNT };
enum BleMode  : uint8_t { BM_OFF = 0, BM_ON_USE, BM_AT_BOOT, BM_COUNT };

// modifier bits (USB HID boot keyboard)
#define HM_LCTRL  0x01
#define HM_LSHIFT 0x02
#define HM_LALT   0x04
#define HM_LGUI   0x08
#define HM_RCTRL  0x10
#define HM_RSHIFT 0x20
#define HM_RALT   0x40
#define HM_RGUI   0x80

// a few HID keyboard usages used by apps directly
#define HK_ENTER  0x28
#define HK_ESC    0x29
#define HK_BKSP   0x2A
#define HK_TAB    0x2B
#define HK_SPACE  0x2C
#define HK_RIGHT  0x4F
#define HK_LEFT   0x50
#define HK_DOWN   0x51
#define HK_UP     0x52
#define HK_PGUP   0x4B
#define HK_PGDN   0x4E
#define HK_F5     0x3E
#define HK_B      0x05
#define HK_L      0x0F

// consumer (media) usages
#define HC_PLAYPAUSE 0x00CD
#define HC_NEXT      0x00B5
#define HC_PREV      0x00B6
#define HC_STOP      0x00B7
#define HC_MUTE      0x00E2
#define HC_VOLUP     0x00E9
#define HC_VOLDOWN   0x00EA

#define HB_LEFT   0x01
#define HB_RIGHT  0x02
#define HB_MIDDLE 0x04

void        hidBegin();              // setup(): USB HID now, BLE per Settings
void        hidLoop();               // every loop iteration: macro playback, battery
void        hidBleStart();           // start Bluetooth HID (idempotent)
bool        hidBleStarted();
bool        hidBleConnected();
bool        hidUsbMounted();
bool        hidReady();              // the routed transport can send right now
const char* hidRouteName();          // "USB" / "Bluetooth" / "-"
void        hidUnpairAll();          // forget every bonded Bluetooth host
void        hidRouteOverride(uint8_t r);   // app-forced route (HR_AUTO = follow Settings)
void        hidToggleRoute();        // quick USB <-> Bluetooth switch (Air Mouse: hold UP)
uint8_t     hidRoute();              // HR_USB / HR_BLE in effect right now
int         hidBondCount();          // paired computers / phones
void        hidBleAdvertise();       // make it discoverable for a new pairing
bool        hidBleAdvertising();

// immediate (real-time) reports
void hidMouseMove(int dx, int dy, int wheel = 0, int pan = 0);
void hidMouseButtons(uint8_t buttons);       // absolute button state
void hidMousePress(uint8_t b);
void hidMouseRelease(uint8_t b);
void hidKeysDown(uint8_t mods, uint8_t usage);   // hold a key (until hidReleaseAll)
void hidReleaseAll();

// queued (macro) actions
void hidTap(uint8_t mods, uint8_t usage);
void hidConsumerTap(uint16_t usage);
void hidType(const char* text);
bool hidRunMacro(const char* macro, char* err = nullptr, size_t errLen = 0);  // parse + queue
bool hidCheckMacro(const char* macro, char* err, size_t errLen);             // parse only
bool hidBusy();                      // a macro is still playing
void hidCancel();                    // drop the queue + release everything
