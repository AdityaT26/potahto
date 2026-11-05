// Potahto: firmware for my XIAO ESP32-S3 multitool.
//
// Sensor tools, IR, air mouse / macro pad / presenter over USB and Bluetooth,
// smart home, desk stuff, a wireless toolkit, games and a buzzer composer.
// Every .cpp includes only Multitool.h. loop() owns the frame and the apps just
// draw into it. Wiring, libraries and tuning are in README.md.
#include "Multitool.h"
#include "Proto.h"

static uint32_t s_lastFrame = 0;

void setup() {
  powerBegin();         // first: release deep-sleep pin holds, read the wake reason
  Serial.begin(115200);
  delay(20);

  settingsBegin();      // load persisted config first (feedback needs it)
  timeBegin();          // time zone; the clock survives sleep / restarts
  storeBegin();         // LittleFS for macros, songs, smart home (survives updates)
  sensorsBegin();       // Wire.begin(custom pins) + probe every I2C device
  uiBegin();            // u8g2.begin() + apply saved rotation
  fbBegin();            // LEDC (motor/buzzer) + neopixel
  inputBegin();         // D-pad

  if (!powerFastBoot()) startupSequence();   // animation + jingle + self-test
  protoBegin();         // config protocol (USB serial, HTTP, Bluetooth)
  hidBegin();           // USB keyboard/mouse/media + Bluetooth HID (per Settings)
  bleLinkBegin();       // web app / companion / phone clock over Bluetooth
  macrosBegin();        // macro-pad profiles + their protocol commands
  musicLibBegin();      // songs, tones, composer storage
  notifyBegin();        // inbox, banner, ring, clock sync
  deskBegin();          // alarms, timer, pomodoro, PC stats
  watchBegin();         // tripwire + desk guard background services
  iotBegin();           // MQTT / Home Assistant / webhooks
  wirelessBegin();      // toolkit: BLE/LAN scan, net doctor, wake-on-LAN
  netBegin();           // WiFi manager (starts per Settings > WiFi)
  appsBegin();          // land on the main menu
  if (powerFastBoot()) inputClearFrame();    // the wake press doesn't also act
}

void loop() {
  uint32_t now = millis();

  // Background services run every iteration, independent of the frame rate.
  netLoop();            // WiFi state machine + web server + OTA
  protoSerialLoop();    // USB config protocol
  bleLinkLoop();        // Bluetooth config protocol + phone time
  hidLoop();            // macro playback + BLE battery level
  if (otaActive()) return;   // firmware update owns the screen

  bool live = (APPS[currentApp()].flags & APPF_LIVE) || pointerActive();
  // dark + idle: no drawing, just poll the buttons often enough to catch a tap
  uint32_t frameMs = (powerScreenOn() || live) ? FRAME_MS : 20;
  if (now - s_lastFrame < frameMs) {
    fbUpdate();
    settingsTick();
    delay(powerScreenOn() ? 1 : 8);           // let the CPU idle between frames
    return;
  }
  s_lastFrame = now;

  inputUpdate();
  powerLoop();          // power gesture, wake, lock screen input, timeouts
  notifyLoop();         // alarm/find-me ring (eats the button press that stops it)
  deskLoop();           // alarms, timer, pomodoro in the background
  watchLoop();          // tripwire + desk guard in the background
  iotLoop();            // MQTT + webhook bookkeeping
  wirelessLoop();       // signal hunter sampling
  sensorsIdleTick();    // sleep sensors nobody is reading
  fbUpdate();
  settingsTick();

  uint8_t fl = APPS[currentApp()].flags;
  if (fl & APPF_STAYON) powerKeepAwake(2); else if (fl & APPF_DIMOK) powerKeepAwake(1);

  // ---- screen off: live apps keep running blind, everything else waits
  if (!powerScreenOn()) {
    if (live && !powerLocked()) {
      uiFrameBegin();                         // draw into the buffer, never send it
      if (pointerActive()) pointerOverlayLoop(); else appsDispatch();
    }
    return;
  }
  if (powerLocked()) {
    uiFrameBegin();
    powerDrawLock();
    notifyOverlay();
    uiFrameEnd();
    return;
  }

  // Games own the whole frame (their own clear + partial-tile flush + timing),
  // so skip auto-rotate and the standard clear/overlay/send path for them.
  if (g_customFrame) {
    appsDispatch();
    return;
  }

  // Auto-rotate: flip the panel when the device is held upside-down.
  if (S.autoRotate && mpuPresent()) {
    Vec3f a, g; float t;
    if (mpuRead(a, g, t)) {
      float mag = sqrtf(a.x * a.x + a.y * a.y);
      if (mag > (S.autoRotateThresh / 10.0f)) {
        uiAutoRotate(atan2f(a.x, a.y) * 57.29578f);
      }
    }
  }

  uiFrameBegin();
  if (pointerActive())    pointerOverlayLoop();   // macro hand-off: tilt-to-point
  else if (kbdActive())   kbdUpdate();            // on-device keyboard
  else if (codeActive())  codeUpdate();           // unlock-code entry
  else if (modalActive()) modalUpdate();
  else                    appsDispatch();
  toastUpdate();
  notifyOverlay();      // notification banner / ring panel
  powerDrawHoldHint();  // "let go: back / keep holding: sleep"
  uiFrameEnd();
}
