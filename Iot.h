// Smart home: MQTT (Home Assistant discovery) + webhooks + events
//
// Everything that happens on Potahto is an EVENT "type/sub", e.g.
//   button/up_short  button/left_long  tripwire/tripped  tripwire/cleared
//   pomodoro/focus   pomodoro/break    pomodoro/done     desk/left
//   desk/returned    alarm/ring        timer/done        boot/online
// Each event is
//   * published to MQTT  <base>/<id>/event  {"event":"tripwire/tripped",...}
//   * a Home Assistant device trigger (auto-discovered, use it in automations)
//   * optionally bound to a webhook (Smart Button keys, or event rules)
// Home Assistant also gets sensors (battery, voltage, temperature, WiFi,
// tripwire, desk presence), a "Ring" button (find my Potahto) and a
// "Message" text box that pops up on the screen.
// Config lives in /iot.json and is edited on the web app's Smart page.
// MQTT keeps WiFi on while enabled. Webhooks wake WiFi on demand. Both run
// in background tasks, so a dead broker or slow server never freezes the UI.
#pragma once
#include <Arduino.h>

#define IOT_HOOKS   6
#define IOT_BUTTONS 9
#define IOT_RULES   8

enum IotMqttState : uint8_t { MQ_OFF = 0, MQ_WAIT_WIFI, MQ_CONNECTING, MQ_CONNECTED, MQ_ERROR };

void         iotBegin();
void         iotLoop();
void         iotEvent(const char* type, const char* sub);   // emit an event (main thread)
void         iotState(const char* what, bool on);          // binary sensors: "tripwire", "presence"
void         iotButton(int idx, bool hold);                 // Smart Button key (0-4 tap, 0-3 hold)
const char*  iotButtonLabel(int i);                         // 0..8 like the macro pad
IotMqttState iotMqttState();
const char*  iotMqttStateName();
bool         iotMqttEnabled();
int          iotLastHookCode();                             // HTTP status, <0 = error, 0 = none
const char*  iotLastHookName();
const char*  iotLastEvent();
const char*  iotDeviceId();
