// USB + BLE composite HID, macro parser, non-blocking playback
#include "Multitool.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include "HidPriv.h"
#include "Proto.h"

#if ARDUINO_USB_MODE == 0 && CONFIG_TINYUSB_HID_ENABLED
  #define HID_HAVE_USB 1
  #include <USB.h>
  #include <USBHIDKeyboard.h>
  #include <USBHIDMouse.h>
  #include <USBHIDConsumerControl.h>
  // Constructed at static-init time so they are part of the USB descriptor
  // before the core starts TinyUSB.
  static USBHIDKeyboard        s_usbKbd;
  static USBHIDMouse           s_usbMouse;
  static USBHIDConsumerControl s_usbCons;
#else
  #define HID_HAVE_USB 0        // "USB Mode: Hardware CDC and JTAG" -> BLE only
#endif

// ---- BLE report map: keyboard (id 1), mouse (id 2), consumer (id 3)
static const uint8_t REPORT_MAP[] = {
  // Keyboard
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08, 0x81, 0x02,                         // 8 modifier bits
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,                         // reserved byte
  0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
  0x91, 0x02,                                                 // 5 LED bits (output)
  0x95, 0x01, 0x75, 0x03, 0x91, 0x01,                         // LED padding
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x73,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x73, 0x81, 0x00,             // 6 keys (incl. F13-F24)
  0xC0,
  // Mouse
  0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00,
  0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01,
  0x95, 0x05, 0x75, 0x01, 0x81, 0x02,                         // 5 buttons
  0x95, 0x01, 0x75, 0x03, 0x81, 0x03,                         // padding
  0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
  0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, // X, Y, wheel
  0x05, 0x0C, 0x0A, 0x38, 0x02,
  0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06, // horizontal pan
  0xC0, 0xC0,
  // Consumer control
  0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03,
  0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03,
  0x95, 0x01, 0x75, 0x10, 0x81, 0x00,                         // one 16-bit usage
  0xC0,
};

static NimBLEServer*         s_srv = nullptr;
static NimBLEHIDDevice*      s_hid = nullptr;
static NimBLECharacteristic* s_inKbd = nullptr;
static NimBLECharacteristic* s_inMouse = nullptr;
static NimBLECharacteristic* s_inCons = nullptr;
static volatile bool         s_bleConn = false;
static volatile uint16_t     s_connHandle = 0xFFFF;
static bool                  s_bleStarted = false;
static uint8_t               s_mouseBtn = 0;
static uint32_t              s_lastBatt = 0;

static volatile bool         s_bleJustConnected = false;
class HidServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* srv, NimBLEConnInfo& ci) override {
    s_bleConn = true; s_connHandle = ci.getConnHandle(); s_bleJustConnected = true;
    srv->updateConnParams(ci.getConnHandle(), 6, 12, 0, 200);   // 7.5-15 ms: snappy pointer
    bleLinkOnConnect(ci);
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    s_bleConn = false; s_connHandle = 0xFFFF;                  // advertising restarts automatically
    bleLinkOnDisconnect();
  }
};
static HidServerCb s_cb;

void hidBleStart() {
  if (s_bleStarted || S.bleMode == BM_OFF) return;
  NimBLEDevice::init(S.devName);
  NimBLEDevice::setSecurityAuth(true, false, true);             // bond, no MITM, secure connections
  s_srv = NimBLEDevice::createServer();
  s_srv->setCallbacks(&s_cb, false);
  s_srv->advertiseOnDisconnect(true);
  s_hid = new NimBLEHIDDevice(s_srv);
  s_inKbd   = s_hid->getInputReport(1);
  s_hid->getOutputReport(1);                                    // keyboard LEDs (ignored)
  s_inMouse = s_hid->getInputReport(2);
  s_inCons  = s_hid->getInputReport(3);
  s_hid->setManufacturer("Potahto");
  s_hid->setPnp(0x02, 0x303A, 0x8160, 0x0120);
  s_hid->setHidInfo(0x00, 0x02);
  s_hid->setReportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
  s_hid->setBatteryLevel(battPercent());
  bleLinkAttach(s_srv);                                         // protocol service (web app / companion)
  s_srv->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(s_hid->getHidService()->getUUID());
  adv->setName(S.devName);
  adv->enableScanResponse(true);
  adv->start();
  s_bleStarted = true;
}
bool hidBleStarted()   { return s_bleStarted; }
bool hidBleConnected() { return s_bleConn; }
void hidUnpairAll() {
  prefs.putBool("bleHad", false);
  if (!s_bleStarted) { NimBLEDevice::init(S.devName); NimBLEDevice::deleteAllBonds(); NimBLEDevice::deinit(true); return; }
  NimBLEDevice::deleteAllBonds();
  if (s_bleConn && s_srv) s_srv->disconnect(s_connHandle);
}
int hidBondCount() { return s_bleStarted ? NimBLEDevice::getNumBonds() : (prefs.getBool("bleHad", false) ? 1 : 0); }
void hidBleAdvertise() {                                         // "pair a new device"
  hidBleStart();
  if (!s_bleStarted) return;
  if (s_bleConn && s_srv) s_srv->disconnect(s_connHandle);
  NimBLEDevice::getAdvertising()->start();
}
bool hidBleAdvertising() { return s_bleStarted && NimBLEDevice::getAdvertising()->isAdvertising(); }

bool hidUsbMounted() {
#if HID_HAVE_USB
  return (bool)USB;
#else
  return false;
#endif
}
static uint8_t s_override = HR_AUTO;
// Auto routing = "the last one that connected wins": plugging in a USB cable
// switches to USB, a Bluetooth host connecting switches to Bluetooth, and if
// only one of them is there, that one is used. (Before, a cable to a PC -
// even one just charging the battery - pinned everything to USB.)
static uint8_t s_lastConn = HR_BLE;
static bool    s_usbWas = false;
void hidRouteOverride(uint8_t r) { s_override = r; }
static HidRoute route() {
  uint8_t want = s_override != HR_AUTO ? s_override : S.hidRoute;
  if (want == HR_USB) return HR_USB;
  if (want == HR_BLE) return HR_BLE;
  bool usb = hidUsbMounted(), ble = s_bleConn;
  if (usb && ble) return (HidRoute)s_lastConn;
  if (usb) return HR_USB;
  if (ble) return HR_BLE;
  return (HidRoute)s_lastConn;
}
void hidToggleRoute() {
  if (S.hidRoute == HR_AUTO) s_lastConn = route() == HR_USB ? HR_BLE : HR_USB;
  else { S.hidRoute = S.hidRoute == HR_USB ? HR_BLE : HR_USB; settingsSaveSoon(); }
  if (route() == HR_BLE) hidBleStart();
}
uint8_t hidRoute() { return route(); }
bool hidReady() { return route() == HR_USB ? hidUsbMounted() : s_bleConn; }
const char* hidRouteName() {
  if (route() == HR_USB) return hidUsbMounted() ? "USB" : "USB (unplugged)";
  if (!s_bleStarted) return S.bleMode == BM_OFF ? "Bluetooth off" : "Bluetooth starting";
  return s_bleConn ? "Bluetooth" : "BT: waiting for PC";
}

// ---- raw senders
static void sendKbd(uint8_t mods, uint8_t key) {
  if (route() == HR_USB) {
#if HID_HAVE_USB
    KeyReport r = { mods, 0, { key, 0, 0, 0, 0, 0 } };
    s_usbKbd.sendReport(&r);
#endif
  } else if (s_bleConn) {
    uint8_t r[8] = { mods, 0, key, 0, 0, 0, 0, 0 };
    s_inKbd->setValue(r, sizeof(r)); s_inKbd->notify();
  }
}
static void sendMouse(int8_t x, int8_t y, int8_t w, int8_t p) {
  if (route() == HR_USB) {
#if HID_HAVE_USB
    s_usbMouse.move(x, y, w, p);
#endif
  } else if (s_bleConn) {
    uint8_t r[5] = { s_mouseBtn, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)p };
    s_inMouse->setValue(r, sizeof(r)); s_inMouse->notify();
  }
}
static void sendButtons() {
  if (route() == HR_USB) {
#if HID_HAVE_USB
    s_usbMouse.buttons(s_mouseBtn);
#endif
  } else sendMouse(0, 0, 0, 0);
}
static void sendCons(uint16_t u) {
  if (route() == HR_USB) {
#if HID_HAVE_USB
    if (u) s_usbCons.press(u); else s_usbCons.release();
#endif
  } else if (s_bleConn) {
    uint8_t r[2] = { (uint8_t)(u & 0xFF), (uint8_t)(u >> 8) };
    s_inCons->setValue(r, sizeof(r)); s_inCons->notify();
  }
}

void hidMouseMove(int dx, int dy, int wheel, int pan) {
  while (dx || dy || wheel || pan) {                      // split big moves into int8 steps
    int sx = clampv(dx, -127, 127), sy = clampv(dy, -127, 127);
    int sw = clampv(wheel, -127, 127), sp = clampv(pan, -127, 127);
    sendMouse(sx, sy, sw, sp);
    dx -= sx; dy -= sy; wheel -= sw; pan -= sp;
  }
}
void hidMouseButtons(uint8_t b) { if (b != s_mouseBtn) { s_mouseBtn = b; sendButtons(); } }
void hidMousePress(uint8_t b)   { hidMouseButtons(s_mouseBtn | b); }
void hidMouseRelease(uint8_t b) { hidMouseButtons(s_mouseBtn & ~b); }
void hidKeysDown(uint8_t mods, uint8_t usage) { sendKbd(mods, usage); }
void hidReleaseAll() { sendKbd(0, 0); sendCons(0); if (s_mouseBtn) hidMouseButtons(0); }

// ---- lifecycle
void hidBegin() {
#if HID_HAVE_USB
  s_usbKbd.begin(); s_usbMouse.begin(); s_usbCons.begin();
  USB.begin();
#endif
  // Bluetooth comes up at boot when set to "Always on", or in "When needed"
  // if a computer was ever paired (so it reconnects by itself, like a mouse)
  if (S.bleMode == BM_AT_BOOT || (S.bleMode == BM_ON_USE && prefs.getBool("bleHad", false))) hidBleStart();
  s_usbWas = hidUsbMounted();
  s_lastConn = s_usbWas ? HR_USB : HR_BLE;
}

void hidLoop() {
  uint32_t now = millis();
  g_bleConnected = s_bleConn;
  bool usb = hidUsbMounted();
  if (usb && !s_usbWas) s_lastConn = HR_USB;                    // cable just plugged into a PC
  s_usbWas = usb;
  if (s_bleJustConnected) {
    s_bleJustConnected = false; s_lastConn = HR_BLE;
    if (!prefs.getBool("bleHad", false)) prefs.putBool("bleHad", true);
    toast("Bluetooth connected"); fbConfirm();
  }
  if (s_bleConn && now - s_lastBatt > 60000) { s_lastBatt = now; s_hid->setBatteryLevel(battPercent(), true); }
  hidMacroTick();
}

// ---- primitives for the macro engine (HidPriv.h)
void hidxKbd(uint8_t mods, uint8_t usage) { sendKbd(mods, usage); }
void hidxCons(uint16_t usage)             { sendCons(usage); }
uint8_t hidxButtons()                     { return s_mouseBtn; }
bool hidxIsUsb()                          { return route() == HR_USB; }
void hidxPointer(uint8_t mode)            { pointerStart(mode, mode == PTR_DRAG ? "Drag to select" : mode == PTR_CLICK ? "Point and click" : "Pointer"); }
void hidxEvent(const char* kind, const char* arg) {
  JsonDocument d; d["event"] = kind; d["cmd"] = arg;
  protoEmit(d.as<JsonObjectConst>());
}
