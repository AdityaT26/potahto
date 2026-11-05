// IR record & replay using the Arduino-IRremote library
//
// Uses IRremote.hpp (the library your receiver test already runs). Decoded
// protocols are stored compactly and re-sent with IrSender.write(); anything
// that doesn't decode (UNKNOWN / pulse-width / pulse-distance) is stored as a
// raw timing array and re-sent with IrSender.sendRaw() at 38 kHz.
//
// IMPORTANT: IRremote.hpp must be included in exactly one .cpp, this one.
//
// Browser: UP/DOWN pick a slot · SELECT tap = transmit (or record if empty)
//          RIGHT = (re)record · LEFT = delete · hold SELECT = back.
#include "Multitool.h"

#if ENABLE_IR
// (leave all protocols enabled; exclude the exotic ones to save flash)
#define EXCLUDE_EXOTIC_PROTOCOLS
#define NO_LED_FEEDBACK_CODE
// Range: a 50 % carrier duty puts ~1.7x more light in every burst than the
// library's 30 % default; TV / AC receivers decode it fine.
#define IR_SEND_DUTY_CYCLE_PERCENT 50
#include <IRremote.hpp>
#include "driver/gpio.h"
#endif

#define IR_SLOTS    6
#define IR_RAW_MAX  250          // max raw timing entries per slot

struct IrRec {
  uint8_t  used;
  uint8_t  isRaw;
  uint8_t  protocol;            // decode_type_t
  uint16_t address;
  uint16_t command;
  uint16_t extra;
  uint16_t numberOfBits;
  uint32_t rawData;             // decodedRawData (for display / some protocols)
  uint16_t rawLen;
  uint8_t  rawCode[IR_RAW_MAX]; // raw ticks (only when isRaw)
};

static IrRec s_slot[IR_SLOTS];
static int   s_sel = 0, s_top = 0;
static bool  s_recording = false;
#if ENABLE_IR
static bool  s_begun = false;
#endif

static void slotKey(int i, char* o) { sprintf(o, "ir%d", i); }

static void loadSlots() {
  for (int i = 0; i < IR_SLOTS; i++) {
    memset(&s_slot[i], 0, sizeof(IrRec));
    char k[6]; slotKey(i, k);
    if (prefs.isKey(k)) prefs.getBytes(k, &s_slot[i], sizeof(IrRec));
  }
}
static void saveSlot(int i) {
  char k[6]; slotKey(i, k);
  prefs.putBytes(k, &s_slot[i], sizeof(IrRec));
}

#if ENABLE_IR
static bool tryCapture(int slot) {
  if (IrReceiver.decodedIRData.rawlen < 4) return false;
  uint8_t f = IrReceiver.decodedIRData.flags;
  if (f & (IRDATA_FLAGS_IS_REPEAT | IRDATA_FLAGS_IS_AUTO_REPEAT)) return false;
  if (f & (IRDATA_FLAGS_PARITY_FAILED | IRDATA_FLAGS_WAS_OVERFLOW)) return false;

  auto& d = IrReceiver.decodedIRData;
  IrRec& r = s_slot[slot];
  if (d.protocol == UNKNOWN || d.protocol == PULSE_WIDTH || d.protocol == PULSE_DISTANCE) {
    uint16_t rl = IrReceiver.decodedIRData.rawlen;   // public field (>= v4.3)
    if ((uint16_t)(rl - 1) > IR_RAW_MAX) return false;         // too long to store
    memset(&r, 0, sizeof(IrRec));
    r.used = 1; r.isRaw = 1; r.rawLen = rl - 1;
    IrReceiver.compensateAndStoreIRResultInArray(r.rawCode);
  } else {
    memset(&r, 0, sizeof(IrRec));
    r.used = 1; r.isRaw = 0;
    r.protocol = (uint8_t)d.protocol;
    r.address = d.address;
    r.command = d.command;
    r.extra = d.extra;
    r.numberOfBits = d.numberOfBits;
    r.rawData = (uint32_t)d.decodedRawData;
  }
  saveSlot(slot);
  return true;
}

static void transmit(int slot) {
  IrRec& r = s_slot[slot];
  if (!r.used) { fbError(); return; }
  npxFlash(strip.Color(0, 80, 200), 1, 120);
  motorPulseStr(20, 6);
  buzzTone(2200, 40);
  IrReceiver.stop();
  // Range: strongest GPIO drive (~40 mA instead of ~20 mA) for an IR LED on
  // the pin through a small resistor, and optional repeats (Settings >
  // Presenter & keys > IR Remote > Extra repeats) so a far / off-axis
  // receiver gets more than one chance. For real distance use a transistor
  // driver: see README > IR range.
  gpio_set_drive_capability((gpio_num_t)PIN_IR_TX, GPIO_DRIVE_CAP_3);
  if (r.isRaw) {
    for (int k = 0; k <= S.irRepeat; k++) {
      if (k) delay(40);
      IrSender.sendRaw(r.rawCode, r.rawLen, 38);
      gpio_set_drive_capability((gpio_num_t)PIN_IR_TX, GPIO_DRIVE_CAP_3);
    }
  } else {
    IRData d;
    memset(&d, 0, sizeof(d));
    d.protocol = (decode_type_t)r.protocol;
    d.address = r.address;
    d.command = r.command;
    d.extra = r.extra;
    d.numberOfBits = r.numberOfBits;
    d.decodedRawData = r.rawData;
    d.flags = IRDATA_FLAGS_EMPTY;
    IrSender.write(&d, (int_fast8_t)S.irRepeat);
  }
  IrReceiver.start();
  toast("Sent");
}
#endif  // ENABLE_IR

void irEnter() {
#if ENABLE_IR
  if (!s_begun) {
    IrReceiver.begin(PIN_IR_RX, DISABLE_LED_FEEDBACK);
    IrSender.begin(PIN_IR_TX);
    gpio_set_drive_capability((gpio_num_t)PIN_IR_TX, GPIO_DRIVE_CAP_3);
    s_begun = true;
  } else {
    IrReceiver.start();
  }
#endif
  loadSlots();
  s_recording = false;
}
void irExit() {
#if ENABLE_IR
  IrReceiver.stop();
#endif
}

static void slotLabelValue(int i, char* val, size_t n) {
  IrRec& r = s_slot[i];
  if (!r.used)       snprintf(val, n, "empty");
  else if (r.isRaw)  snprintf(val, n, "raw %d", r.rawLen);
  else               snprintf(val, n, "0x%X", r.command);
}

void irLoop() {
#if !ENABLE_IR
  uiHeader("IR Remote");
  u8g2.setFont(F_UI); uiCenter(38, "IR disabled");
  if (wantBack()) { fbBack(); goBack(); }
  return;
#else
  // ---- recording
  if (s_recording) {
    if (wantBack() || btnTap(BTN_LEFT)) { s_recording = false; fbCancel(); return; }
    if (IrReceiver.decode()) {
      bool got = tryCapture(s_sel);
      IrReceiver.resume();
      if (got) { s_recording = false; fbConfirm(); toast("Recorded"); }
    }
    npxSetBase(NPX_BLINK, strip.Color(200, 0, 0), 8);
    uiHeader("Recording");
    u8g2.setFont(F_MEDB);
    char t[14]; snprintf(t, sizeof(t), "Slot #%d", s_sel + 1);
    uiCenter(30, t);
    u8g2.setFont(F_SMALL);
    uiCenter(44, "point remote, press");
    int dots = (millis() / 300) % 4;
    char d[5] = "...."; d[dots] = 0;
    uiCenter(54, d);
    u8g2.setFont(F_TINY); uiRight(126, 63, "hold SEL=cancel");
    return;
  }

  // ---- browser
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_sel < IR_SLOTS - 1) { s_sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_sel > 0) { s_sel--; fbNav(); } else fbEdge(); }

  if (btnTap(BTN_SELECT)) {
    if (s_slot[s_sel].used) transmit(s_sel);
    else { s_recording = true; IrReceiver.resume(); fbSelect(); }
  }
  if (btnTap(BTN_RIGHT)) { s_recording = true; IrReceiver.resume(); fbSelect(); }
  if (btnTap(BTN_LEFT)) {
    if (s_slot[s_sel].used) {
      char k[6]; slotKey(s_sel, k); prefs.remove(k);
      memset(&s_slot[s_sel], 0, sizeof(IrRec));
      fbConfirm(); toast("Deleted");
    } else fbError();
  }

  npxSetBase(NPX_SOLID, strip.Color(40, 20, 60), 4);

  int rows = uiListRows();
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + rows) s_top = s_sel - rows + 1;

  uiHeader("IR Remote");
  for (int r = 0; r < rows; r++) {
    int i = s_top + r;
    if (i >= IR_SLOTS) break;
    char lab[10], val[14];
    snprintf(lab, sizeof(lab), "Slot %d", i + 1);
    slotLabelValue(i, val, sizeof(val));
    uiListRow(r, lab, val, i == s_sel);
  }
  uiScrollbar(IR_SLOTS, s_top, rows);
#endif
}
