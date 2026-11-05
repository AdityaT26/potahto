// WiFi manager: saved networks, setup AP, mDNS, NTP, network OTA
//
// Policy (Settings > WiFi):
//   Off        - radio never starts (USB config still works)
//   On demand  - radio runs while something needs it (netAcquire/netRelease),
//                then lingers 60 s and powers down (saves battery)
//   Always on  - stays connected (needed for Home Assistant / desk features)
//
// Connecting: an async scan picks the strongest of the (up to 5) saved
// networks that is actually in range, so absent networks cost no time.
// Setup mode: soft-AP "<name>-XXXX" + captive portal; the web app's WiFi page
// opens automatically on phones/PCs that join it.
// When online: http://<name>.local web app + API, NTP time, Arduino IDE
// network upload (password = web PIN).
#pragma once
#include <Arduino.h>

enum NetState : uint8_t {
  NET_OFF = 0,     // radio off
  NET_SCANNING,    // looking for a saved network
  NET_CONNECTING,  // joining one
  NET_ONLINE,      // connected (STA)
  NET_NO_CONFIG,   // wanted, but no networks saved
  NET_FAILED,      // none reachable; retries with back-off
};

void        netBegin();             // setup(): after settingsBegin()
void        netLoop();              // every loop iteration (cheap)

void        netAcquire();           // "I need WiFi" (ref-counted)
void        netRelease();
void        netReconnect();         // credentials/mode changed -> re-evaluate
void        netSetupAP(bool on);    // setup access point + captive portal
bool        netSetupAPActive();

NetState    netState();
bool        netOnline();
const char* netStateName();
const char* netSsid();              // connected SSID ("" if none)
const char* netIp();                // STA IP, or AP IP in setup mode
int         netRssi();
const char* netHostname();          // sanitised devName, e.g. "potahto"
const char* netApSsid();
const char* netApPass();
bool        netTimeValid();         // NTP has set the clock
// Async scan (web app WiFi page). Results also refresh on every auto-connect.
#define NET_MAX_APS 40
struct NetAP { char ssid[33]; int8_t rssi; uint8_t channel; uint8_t enc; uint8_t bssid[6]; };
void         netScanStart();
const NetAP* netScanResults(int* n);   // n: -2 never scanned, -1 running, else count
void         netExternalScan(bool on);  // a tool drives the radio's scanner: pause ours
const uint8_t* netBssid();              // connected AP's BSSID (nullptr if offline)
int netChannel();                       // connected AP channel (0 if offline)
// Saved networks (Settings > WiFi on the device, or the web app)
int  netSaveNetwork(const char* ssid, const char* pass, const char* user, uint8_t eap);  // -> slot; connects to it
void netForget(int slot);
void netConnectSlot(int slot);          // try this one first, now
int  netCurrentSlot();                  // slot we're connected to, -1 if offline
bool netEncEnterprise(uint8_t enc);     // scan result needs a username (campus / office)
bool netEncOpen(uint8_t enc);

// Firmware-update progress (network OTA from the web app or Arduino IDE).
// The main loop shows a full-screen progress view while this is active.
bool        otaActive();
void        otaSetActive(bool on);
void        otaDrawProgress(const char* title, uint32_t done, uint32_t total);

// ---- HTTP server (WebSrv.cpp); started/stopped by the WiFi state machine
void webStart();
void webStop();
void webLoop();
