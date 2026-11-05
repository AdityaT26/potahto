// Wireless toolkit back end (for your networks and devices)
//
// * Signal hunter : live RSSI of one access point (dead-zone hunting,
//                   router placement)
// * BLE scanner   : nearby Bluetooth LE advertisers + finder for a lost
//                   earbud / tag you own
// * LAN scan      : mDNS service discovery on the network you're joined to
// * Net doctor    : link, gateway ping, DNS, internet reachability, latency
// * Wake-on-LAN   : magic packets to your PCs (/wol.json, web app > Toolkit)
// Everything here only listens or talks to your own gear: there are no
// deauth, jamming, spoofing or capture features, by design.
#pragma once
#include <Arduino.h>

void wirelessBegin();               // protocol commands
void wirelessLoop();

// ---- signal hunter
void hunterStart(const uint8_t bssid[6], uint8_t channel, const char* ssid);
void hunterStop();
bool hunterActive();
int  hunterRssi();                  // latest dBm, -127 = not seen recently
const char* hunterSsid();

// ---- BLE
#define BLE_MAX 48
struct BleDev {
  uint8_t  addr[6]; char name[20]; int8_t rssi; float rssiAvg;
  uint16_t company; uint8_t kind;  // BleKind
  uint32_t seen;                    // millis()
};
enum BleKind : uint8_t { BK_NONE = 0, BK_IBEACON, BK_EDDYSTONE, BK_FINDMY };
void        bleScanStart();
void        bleScanStop();
bool        bleScanning();
int         bleSnapshot(BleDev* out, int max);   // copy, sorted strongest first
bool        bleLookup(const uint8_t addr[6], BleDev& out);
const char* bleCompanyName(uint16_t id);
const char* bleKindName(uint8_t k);

// ---- LAN
#define LAN_MAX 32
struct LanHost { char host[32]; uint32_t ip; uint16_t port; uint8_t svc; };
void        lanScanStart();
bool        lanScanning();
int         lanProgress();          // 0..100
int         lanSnapshot(LanHost* out, int max);
const char* lanServiceName(uint8_t i);

// ---- net doctor
enum DocStep : uint8_t { DS_LINK = 0, DS_GATEWAY, DS_DNS, DS_INTERNET, DS_LATENCY, DS_COUNT };
struct DocResult { int8_t state; int value; char note[24]; };   // state: 0 pending, 1 ok, 2 warn, 3 fail
void             doctorRun();
bool             doctorRunning();
const DocResult* doctorResults();
const char*      doctorStepName(uint8_t i);

// ---- wake-on-LAN
#define WOL_MAX 6
int         wolCount();
const char* wolName(int i);
bool        wolSend(int i);
