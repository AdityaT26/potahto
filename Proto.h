// Potahto config protocol (JSON request/response)
//
// One protocol, two transports:
//   USB serial : one JSON object per line in, one JSON line out (115200 8N1;
//                the baud rate is ignored on native USB). Trusted: physical
//                access, so no PIN needed.
//   HTTP       : POST /api with the same JSON body. Requires the 6-digit web
//                PIN (header "X-Pin" or field "pin"); 5 wrong tries = 60 s lock.
//
// Request : {"id":1,"cmd":"set","values":{"mSens":9}}
// Response: {"id":1,"ok":true,...}   or   {"id":1,"ok":false,"err":"..."}
//
// Modules add their own commands with protoRegister() (see Proto.cpp for the
// core set: hello, auth, status, schema, set, save, reboot, factory, backup,
// restore, wifi.*, hiscores, toast, cal.*).
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

enum ProtoVia : uint8_t { PV_SERIAL = 0, PV_HTTP, PV_BLE };
struct ProtoCtx { ProtoVia via; bool authed; };

// Handler: fill `res`; return false and set res["err"] on failure.
typedef bool (*ProtoFn)(JsonObjectConst req, JsonObject res, ProtoCtx& ctx);

void protoBegin();
void protoRegister(const char* cmd, ProtoFn fn, bool needAuth = true);
void protoHandle(const char* body, size_t len, String& out, ProtoCtx& ctx);
void protoSerialLoop();                  // call every loop iteration
bool protoPinCheck(const char* pin);     // rate-limited PIN check (HTTP)

// Unsolicited events to connected apps (companion, web app): one JSON object,
// e.g. {"event":"pc.run","cmd":"..."}. Goes out over USB serial (if a host has
// the port open), Bluetooth (if subscribed) and the WiFi poll queue.
void protoEmit(JsonObjectConst ev);

// helpers for modules
void protoFieldValue(JsonObject dst, const SettingField& f, bool withSecrets);
void protoApplySideEffects(const char* key);
