// Smart Button + Smart Status
//
// Smart Button: every key is a smart-home trigger. Tap UP/DOWN/LEFT/RIGHT/
// SELECT or hold a direction -> Home Assistant device trigger + MQTT event +
// the webhook bound to that key (web app > Smart). Hold SELECT = back.
// Smart Status: broker connection, Home Assistant id, last event / webhook.
#include "Multitool.h"

static int8_t   s_flash = -1;
static uint32_t s_flashUntil = 0;

void smartEnter() {}

static void cellS(int cx, int y, int w, int k) {
  const char* t = iotButtonLabel(k); if (!t[0]) t = "-";
  bool hi = (s_flash == k) && millis() < s_flashUntil;
  char buf[14]; u8g2.setFont(F_SMALL); uiTruncate(buf, sizeof(buf), t, w - 4);
  if (hi) { u8g2.drawRBox(cx - w / 2, y - 8, w, 11, 2); u8g2.setDrawColor(0); }
  else u8g2.drawRFrame(cx - w / 2, y - 8, w, 11, 2);
  u8g2.drawStr(cx - u8g2.getStrWidth(buf) / 2, y, buf);
  u8g2.setDrawColor(1);
}
static void press(int idx, bool hold) {
  iotButton(idx, hold);
  s_flash = hold ? 5 + idx : idx; s_flashUntil = millis() + 220;
  fbSelect();
  npxFlash(strip.Color(200, 120, 0), 1, 150);
}

void smartLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  static const uint8_t DIRS[4] = { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT };
  for (int d = 0; d < 4; d++) {
    if (btnTap(DIRS[d]))  press(d, false);
    if (btnLong(DIRS[d])) press(d, true);
  }
  if (btnTap(BTN_SELECT)) press(4, false);
  bool holdLayer = false;
  for (int d = 0; d < 4; d++) if (btnHeldMs(DIRS[d]) > 200) holdLayer = true;
  int o = holdLayer ? 5 : 0;

  uiHeader("Smart Button");
  cellS(64, 23, 50, o + 0);
  cellS(22, 38, 42, o + 2);
  if (!holdLayer) cellS(64, 38, 40, 4); else { u8g2.setFont(F_TINY); uiCenter(37, "HOLD"); }
  cellS(106, 38, 42, o + 3);
  cellS(64, 53, 50, o + 1);
  u8g2.setFont(F_TINY);
  char st[28];
  if (iotMqttEnabled()) snprintf(st, sizeof(st), "MQTT %s", iotMqttState() == MQ_CONNECTED ? "ok" : "...");
  else snprintf(st, sizeof(st), "webhooks");
  u8g2.drawStr(0, 63, st);
  int code = iotLastHookCode();
  if (code) {
    char c[16]; if (code > 0) snprintf(c, sizeof(c), "HTTP %d", code); else snprintf(c, sizeof(c), code == -3 ? "no WiFi" : "failed");
    uiRight(127, 63, c);
  }
  npxSetBase(NPX_BREATHE, iotMqttState() == MQ_CONNECTED ? strip.Color(0, 120, 60) : strip.Color(120, 70, 0), 3);
}

// ---- status
void smartStatusEnter() {}
void smartStatusLoop() {
  if (wantBack() || btnTap(BTN_LEFT)) { fbBack(); goBack(); return; }
  uiHeader("Smart Status");
  u8g2.setFont(F_SMALL);
  char l[40];
  snprintf(l, sizeof(l), "MQTT: %s", iotMqttEnabled() ? iotMqttStateName() : "off");       u8g2.drawStr(2, 20, l);
  snprintf(l, sizeof(l), "WiFi: %s", netStateName());                                       u8g2.drawStr(2, 28, l);
  snprintf(l, sizeof(l), "ID: %s", iotDeviceId());                                          u8g2.drawStr(2, 36, l);
  snprintf(l, sizeof(l), "Last: %s", iotLastEvent()[0] ? iotLastEvent() : "-");             uiTextClipped(2, 44, l, 124);
  int code = iotLastHookCode();
  if (code) {
    if (code > 0) snprintf(l, sizeof(l), "Hook %s: %d", iotLastHookName(), code);
    else snprintf(l, sizeof(l), "Hook %s: %s", iotLastHookName(), code == -3 ? "no WiFi" : "failed");
  } else snprintf(l, sizeof(l), "Hook: none sent yet");
  uiTextClipped(2, 52, l, 124);
  u8g2.setFont(F_TINY); uiCenter(63, "set up in web app > Smart");
}
