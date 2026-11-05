// Notifications inbox + banner, and the "ring" (alarm / find-me)
//
// Sources: web app, HTTP/serial API ("notify"), Home Assistant / MQTT
// (<base>/<id>/cmd/notify or .../cmd/message), the PC companion script.
// A banner slides over whatever app is open; the Inbox app lists the last 10.
// Settings > Notifications > Alert: Silent / Vibrate / Sound.
//
// ringStart() is the loud attention mode used by alarms, timers and
// "Find my Potahto": siren + vibration + flashing LED, ignores the global
// buzzer mute, and any button stops it (that press is swallowed).
#pragma once
#include <Arduino.h>

#define NOTIFY_MAX 10
struct Note { char title[24]; char text[100]; char src[10]; uint32_t at; bool read; };

void        notifyBegin();          // protocol commands: notify, notify.list, notify.clear, time.set, ring
void        notifyLoop();           // ring timing; call every frame (before apps)
void        notifyPush(const char* title, const char* text, const char* src);
int         notifyCount();
const Note& notifyGet(int i);       // 0 = newest
int         notifyUnread();
void        notifyMarkAllRead();
void        notifyClear();
void        notifyOverlay();        // draw banner / ring panel (main loop, after toast)

void        ringStart(const char* title, const char* sub, uint32_t ms, uint8_t tone = 0);   // tone: ToneKind
void        ringStop();
bool        ringActive();
