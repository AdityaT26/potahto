// Protocol over Bluetooth LE (Nordic UART service) + phone clock sync
#include "Multitool.h"
#include "Proto.h"
#include <NimBLEDevice.h>

#define NUS_SVC "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_LINE_MAX 4096

static NimBLEServer*         s_srv = nullptr;
static NimBLECharacteristic* s_tx = nullptr;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static char     s_in[BLE_LINE_MAX]; static size_t s_inLen = 0; static bool s_inReady = false, s_inOver = false;
static uint16_t s_conn = 0xFFFF; static bool s_sub = false, s_authed = false, s_enc = false;
static uint32_t s_connAt = 0; static bool s_peerQueried = false;
static char     s_peer[32] = "";

class RxCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& ci) override {
    NimBLEAttValue v = c->getValue();
    portENTER_CRITICAL(&s_mux);
    s_enc = ci.isEncrypted();
    for (size_t i = 0; i < v.size() && !s_inReady; i++) {
      char ch = (char)v[i];
      if (ch == '\r') continue;
      if (ch == '\n') { if (s_inLen) s_inReady = true; break; }
      if (s_inLen < BLE_LINE_MAX - 1) s_in[s_inLen++] = ch; else s_inOver = true;
    }
    portEXIT_CRITICAL(&s_mux);
  }
};
class TxCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& ci, uint16_t sub) override { s_sub = sub != 0; s_conn = ci.getConnHandle(); s_enc = ci.isEncrypted(); }
};
static RxCb s_rxCb; static TxCb s_txCb;

void bleLinkAttach(NimBLEServer* srv) {
  s_srv = srv;
  NimBLEService* svc = srv->createService(NUS_SVC);
  NimBLECharacteristic* rx = svc->createCharacteristic(NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(&s_rxCb);
  s_tx = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
  s_tx->setCallbacks(&s_txCb);
}
void bleLinkOnConnect(NimBLEConnInfo& ci) {
  s_conn = ci.getConnHandle(); s_authed = false; s_sub = false; s_enc = ci.isEncrypted();
  s_connAt = millis(); s_peerQueried = false; s_peer[0] = 0;
  portENTER_CRITICAL(&s_mux); s_inLen = 0; s_inReady = false; s_inOver = false; portEXIT_CRITICAL(&s_mux);
}
void bleLinkOnDisconnect() { s_conn = 0xFFFF; s_sub = false; s_authed = false; }
bool bleLinkClient() { return s_sub && s_conn != 0xFFFF; }
const char* blePeerName() { return s_peer; }

static void sendLine(const char* line, size_t n) {
  if (!s_tx || s_conn == 0xFFFF || !s_sub) return;
  uint16_t mtu = s_srv ? s_srv->getPeerMTU(s_conn) : 23;
  size_t chunk = mtu > 23 ? mtu - 3 : 20;
  if (chunk > 244) chunk = 244;
  for (size_t off = 0; off < n; off += chunk) {
    size_t len = min(chunk, n - off);
    for (int tries = 0; tries < 20; tries++) {           // ride out a full TX buffer
      if (s_tx->notify((const uint8_t*)line + off, len, s_conn)) break;
      delay(5);
    }
  }
  const uint8_t nl = '\n';
  s_tx->notify(&nl, 1, s_conn);
}
void protoEmitBle(const char* line) { sendLine(line, strlen(line)); }

// read the connected host's name and (if it has one) its Current Time Service
static void queryPeer() {
  s_peerQueried = true;
  if (!s_srv || s_conn == 0xFFFF) return;
  NimBLEConnInfo ci = s_srv->getPeerInfoByHandle(s_conn);
  NimBLEClient* c = s_srv->getClient(ci);
  if (!c) return;
  NimBLERemoteService* gap = c->getService(NimBLEUUID((uint16_t)0x1800));
  if (gap) {
    NimBLERemoteCharacteristic* nm = gap->getCharacteristic(NimBLEUUID((uint16_t)0x2A00));
    if (nm && nm->canRead()) {
      NimBLEAttValue v = nm->readValue();
      size_t L = min<size_t>(v.size(), sizeof(s_peer) - 1); memcpy(s_peer, v.data(), L); s_peer[L] = 0;
    }
  }
  NimBLERemoteService* cts = c->getService(NimBLEUUID((uint16_t)0x1805));
  if (cts) {
    NimBLERemoteCharacteristic* ct = cts->getCharacteristic(NimBLEUUID((uint16_t)0x2A2B));
    if (ct && ct->canRead()) {
      NimBLEAttValue v = ct->readValue();
      if (v.size() >= 7) {
        const uint8_t* d = v.data();
        int y = d[0] | (d[1] << 8);
        if (y >= 2024) { timeSetLocalS(y, d[2], d[3], d[4], d[5], d[6], "Phone"); }
      }
    }
  }
}

void bleLinkLoop() {
  if (s_conn != 0xFFFF && !s_peerQueried && millis() - s_connAt > 2500) queryPeer();
  if (!s_inReady && !s_inOver) return;
  static char line[BLE_LINE_MAX];
  size_t n; bool over;
  portENTER_CRITICAL(&s_mux);
  n = s_inLen; over = s_inOver; memcpy(line, s_in, n); line[n] = 0;
  s_inLen = 0; s_inReady = false; s_inOver = false;
  portEXIT_CRITICAL(&s_mux);
  if (over) { const char* e = "{\"ok\":false,\"err\":\"line too long\"}"; sendLine(e, strlen(e)); return; }
  if (!n || line[0] != '{') return;
  ProtoCtx ctx{ PV_BLE, s_enc || s_authed };
  String out;
  protoHandle(line, n, out, ctx);
  if (ctx.authed && !(s_enc || s_authed)) s_authed = true;       // a good "auth" unlocks this link
  sendLine(out.c_str(), out.length());
}
void bleLinkBegin() {}
