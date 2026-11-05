// Potahto's protocol over Bluetooth LE + phone clock sync
//
// The same JSON protocol as USB / WiFi, on a Nordic-UART-style GATT service
// next to the HID keyboard/mouse, so the web app (Chrome/Edge: "Connect over
// Bluetooth") and the PC companion (--ble) work without a cable or WiFi.
//   service 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
//   RX (write)  6E400002-...   one JSON request per line ("\n")
//   TX (notify) 6E400003-...   responses + events, one JSON per line
// Trust: a bonded (encrypted) link is trusted like USB; otherwise send
// {"cmd":"auth","pin":"123456"} first, exactly like WiFi.
// Time: when an iPhone (or anything with the Current Time Service) connects,
// Potahto reads its clock. The web app / companion send theirs too.
#pragma once
#include <Arduino.h>

class NimBLEServer;
class NimBLEConnInfo;
void bleLinkBegin();
void bleLinkAttach(NimBLEServer* srv);       // Hid.cpp, before the server starts
void bleLinkOnConnect(NimBLEConnInfo& ci);
void bleLinkOnDisconnect();
void bleLinkLoop();                          // every loop iteration
bool bleLinkClient();                        // an app is subscribed to TX
const char* blePeerName();                   // name of the connected host ("" if unknown)
void protoEmitBle(const char* line);         // push one JSON line to the subscriber
