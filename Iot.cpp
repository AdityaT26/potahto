// MQTT + Home Assistant discovery + HTTPS webhooks + event bus
#include "Multitool.h"
#include "Proto.h"
#include <WiFi.h>
#include <time.h>
#include "mqtt_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define IOT_FILE "/iot.json"

struct Hook { char name[20]; uint8_t method; char url[200]; char body[300]; char ctype[40]; };
struct Rule { char ev[28]; int8_t hook; };
static struct Cfg {
  bool     mqttOn; char host[64]; uint16_t port; char user[40]; char pass[64];
  char     base[24]; bool ha; char haPrefix[24];
  Hook     hooks[IOT_HOOKS]; uint8_t nHooks;
  char     btnLabel[IOT_BUTTONS][12]; int8_t btnHook[IOT_BUTTONS];
  Rule     rules[IOT_RULES]; uint8_t nRules;
} C;

static const char* METHODS[3] = { "GET", "POST", "PUT" };
static const char* BTN_NAME[5] = { "up", "down", "left", "right", "select" };

// events we know about (web-app dropdowns + Home Assistant triggers)
static const char* const SYS_EVENTS[] = {
  "tripwire/tripped", "tripwire/cleared", "pomodoro/focus", "pomodoro/break", "pomodoro/done",
  "desk/left", "desk/returned", "alarm/ring", "timer/done", "boot/online",
};

static char s_id[24], s_root[64];
static char s_lastEvent[32] = "";

// ---- config
static void cfgDefaults() {
  memset(&C, 0, sizeof(C));
  C.port = 1883; strcpy(C.base, "potahto"); C.ha = true; strcpy(C.haPrefix, "homeassistant");
  for (int i = 0; i < IOT_BUTTONS; i++) { snprintf(C.btnLabel[i], sizeof(C.btnLabel[i]), i < 5 ? "Scene %d" : "Hold %d", i < 5 ? i + 1 : i - 4); C.btnHook[i] = -1; }
}
static void cfgFromJson(JsonObjectConst j, bool keepPass) {
  char oldPass[sizeof(C.pass)]; strlcpy(oldPass, C.pass, sizeof(oldPass));
  cfgDefaults();
  JsonObjectConst m = j["mqtt"];
  C.mqttOn = m["on"] | false;
  strlcpy(C.host, m["host"] | "", sizeof(C.host));
  C.port = m["port"] | 1883;
  strlcpy(C.user, m["user"] | "", sizeof(C.user));
  if (m["pass"].is<const char*>()) strlcpy(C.pass, m["pass"], sizeof(C.pass));
  else if (keepPass) strlcpy(C.pass, oldPass, sizeof(C.pass));
  strlcpy(C.base, m["base"] | "potahto", sizeof(C.base));
  C.ha = m["ha"] | true;
  strlcpy(C.haPrefix, m["haPrefix"] | "homeassistant", sizeof(C.haPrefix));
  for (JsonObjectConst h : j["hooks"].as<JsonArrayConst>()) {
    if (C.nHooks >= IOT_HOOKS) break;
    Hook& k = C.hooks[C.nHooks++];
    strlcpy(k.name, h["name"] | "Webhook", sizeof(k.name));
    const char* meth = h["method"] | "POST";
    k.method = !strcasecmp(meth, "GET") ? 0 : !strcasecmp(meth, "PUT") ? 2 : 1;
    strlcpy(k.url, h["url"] | "", sizeof(k.url));
    strlcpy(k.body, h["body"] | "", sizeof(k.body));
    strlcpy(k.ctype, h["type"] | "application/json", sizeof(k.ctype));
  }
  int i = 0;
  for (JsonObjectConst b : j["buttons"].as<JsonArrayConst>()) {
    if (i >= IOT_BUTTONS) break;
    strlcpy(C.btnLabel[i], b["label"] | "", sizeof(C.btnLabel[i]));
    C.btnHook[i] = b["hook"] | -1; i++;
  }
  for (JsonObjectConst r : j["rules"].as<JsonArrayConst>()) {
    if (C.nRules >= IOT_RULES) break;
    strlcpy(C.rules[C.nRules].ev, r["event"] | "", sizeof(C.rules[0].ev));
    C.rules[C.nRules].hook = r["hook"] | -1; C.nRules++;
  }
}
static void cfgToJson(JsonObject j, bool withPass) {
  JsonObject m = j["mqtt"].to<JsonObject>();
  m["on"] = C.mqttOn; m["host"] = C.host; m["port"] = C.port; m["user"] = C.user;
  if (withPass) m["pass"] = C.pass; else m["passSet"] = C.pass[0] != 0;
  m["base"] = C.base; m["ha"] = C.ha; m["haPrefix"] = C.haPrefix;
  JsonArray hs = j["hooks"].to<JsonArray>();
  for (int i = 0; i < C.nHooks; i++) {
    JsonObject h = hs.add<JsonObject>();
    h["name"] = C.hooks[i].name; h["method"] = METHODS[C.hooks[i].method % 3];
    h["url"] = C.hooks[i].url; h["body"] = C.hooks[i].body; h["type"] = C.hooks[i].ctype;
  }
  JsonArray bs = j["buttons"].to<JsonArray>();
  for (int i = 0; i < IOT_BUTTONS; i++) { JsonObject b = bs.add<JsonObject>(); b["label"] = C.btnLabel[i]; b["hook"] = C.btnHook[i]; }
  JsonArray rs = j["rules"].to<JsonArray>();
  for (int i = 0; i < C.nRules; i++) { JsonObject r = rs.add<JsonObject>(); r["event"] = C.rules[i].ev; r["hook"] = C.rules[i].hook; }
}
static bool cfgSave() {
  JsonDocument d; cfgToJson(d.to<JsonObject>(), true);
  String s; serializeJson(d, s);
  return storeWrite(IOT_FILE, s.c_str(), s.length());
}
static void cfgLoad() {
  cfgDefaults();
  String s; JsonDocument d;
  if (storeRead(IOT_FILE, s) && !deserializeJson(d, s)) cfgFromJson(d.as<JsonObjectConst>(), false);
}

const char* iotButtonLabel(int i) { return (i >= 0 && i < IOT_BUTTONS) ? C.btnLabel[i] : ""; }
bool        iotMqttEnabled()      { return C.mqttOn && C.host[0]; }
const char* iotDeviceId()         { return s_id; }
const char* iotLastEvent()        { return s_lastEvent; }

// ---- webhooks (worker task)
struct HookReq { char name[20]; uint8_t method; char url[200]; char body[340]; char ctype[40]; };
static QueueHandle_t s_hq = nullptr;
static volatile uint32_t s_hookQueued = 0, s_hookDone = 0;
static volatile int s_lastCode = 0;
static char s_lastHook[20] = "";
static bool s_hookNet = false;

static void hookTask(void*) {
  HookReq* r = (HookReq*)malloc(sizeof(HookReq));
  for (;;) {
    if (xQueueReceive(s_hq, r, portMAX_DELAY) != pdTRUE) continue;
    uint32_t t0 = millis();
    while (!netOnline() && millis() - t0 < 20000) vTaskDelay(pdMS_TO_TICKS(200));
    int code = -3;                                         // -3: no WiFi
    if (netOnline()) {
      esp_http_client_config_t cfg = {};
      cfg.url = r->url;
      cfg.method = r->method == 0 ? HTTP_METHOD_GET : r->method == 2 ? HTTP_METHOD_PUT : HTTP_METHOD_POST;
      cfg.timeout_ms = 8000;
      cfg.crt_bundle_attach = esp_crt_bundle_attach;      // verified HTTPS (Mozilla root bundle)
      cfg.user_agent = "Potahto";
      esp_http_client_handle_t c = esp_http_client_init(&cfg);
      code = -2;                                          // -2: request failed
      if (c) {
        if (r->method != 0 && r->body[0]) {
          esp_http_client_set_header(c, "Content-Type", r->ctype[0] ? r->ctype : "application/json");
          esp_http_client_set_post_field(c, r->body, strlen(r->body));
        }
        if (esp_http_client_perform(c) == ESP_OK) code = esp_http_client_get_status_code(c);
        esp_http_client_cleanup(c);
      }
    }
    strlcpy(s_lastHook, r->name, sizeof(s_lastHook));
    s_lastCode = code;
    s_hookDone = s_hookDone + 1;
  }
}
int         iotLastHookCode() { return s_lastCode; }
const char* iotLastHookName() { return s_lastHook; }

static void expand(const char* in, char* out, size_t n, const char* event) {
  char bat[8]; snprintf(bat, sizeof(bat), "%u", battPercent());
  char ts[24] = "";
  if (timeValid()) { time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt); strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &lt); }
  size_t o = 0;
  for (const char* p = in; *p && o < n - 1; ) {
    const char* rep = nullptr; size_t skip = 0;
    if (!strncmp(p, "{event}", 7))   { rep = event;     skip = 7; }
    else if (!strncmp(p, "{name}", 6))    { rep = S.devName; skip = 6; }
    else if (!strncmp(p, "{battery}", 9)) { rep = bat;       skip = 9; }
    else if (!strncmp(p, "{time}", 6))    { rep = ts;        skip = 6; }
    if (rep) { for (const char* q = rep; *q && o < n - 1; q++) out[o++] = *q; p += skip; }
    else out[o++] = *p++;
  }
  out[o] = 0;
}
static bool fireHook(int idx, const char* event) {
  if (idx < 0 || idx >= C.nHooks || !C.hooks[idx].url[0] || !s_hq) return false;
  static HookReq r;
  const Hook& h = C.hooks[idx];
  strlcpy(r.name, h.name, sizeof(r.name));
  r.method = h.method;
  expand(h.url, r.url, sizeof(r.url), event);
  expand(h.body, r.body, sizeof(r.body), event);
  strlcpy(r.ctype, h.ctype, sizeof(r.ctype));
  if (xQueueSend(s_hq, &r, 0) != pdTRUE) return false;
  s_hookQueued = s_hookQueued + 1;
  return true;
}

// ---- MQTT
static esp_mqtt_client_handle_t s_mq = nullptr;
static volatile bool s_mqConnected = false, s_mqError = false, s_announce = false;
static IotMqttState s_mqState = MQ_OFF;
static bool s_mqNet = false, s_bootSent = false;
static uint32_t s_lastState = 0;
static char s_uri[96];
static char s_lwt[80];
static bool s_trip = false, s_presence = false;

struct Inbound { char topic[96]; char data[160]; };
static Inbound s_in[4];
static volatile uint8_t s_inHead = 0, s_inTail = 0;
static portMUX_TYPE s_inMux = portMUX_INITIALIZER_UNLOCKED;

static void mqttHandler(void*, esp_event_base_t, int32_t id, void* data) {
  esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:    s_mqConnected = true; s_mqError = false; s_announce = true; break;
    case MQTT_EVENT_DISCONNECTED: s_mqConnected = false; break;
    case MQTT_EVENT_ERROR:        s_mqError = true; break;
    case MQTT_EVENT_DATA:
      if (e->current_data_offset == 0 && e->data_len == e->total_data_len) {
        portENTER_CRITICAL(&s_inMux);
        uint8_t next = (s_inTail + 1) % 4;
        if (next != s_inHead) {
          Inbound& m = s_in[s_inTail];
          int tl = min(e->topic_len, (int)sizeof(m.topic) - 1); memcpy(m.topic, e->topic, tl); m.topic[tl] = 0;
          int dl = min(e->data_len, (int)sizeof(m.data) - 1);   memcpy(m.data, e->data, dl);   m.data[dl] = 0;
          s_inTail = next;
        }
        portEXIT_CRITICAL(&s_inMux);
      }
      break;
    default: break;
  }
}

static void pub(const char* sub, const char* payload, bool retain, int qos = 0) {
  if (!s_mq || !s_mqConnected) return;
  char t[128]; snprintf(t, sizeof(t), "%s/%s", s_root, sub);
  esp_mqtt_client_publish(s_mq, t, payload, 0, qos, retain ? 1 : 0);
}

static void devBlock(JsonObject o) {
  JsonObject d = o["dev"].to<JsonObject>();
  d["ids"].to<JsonArray>().add(s_id);
  d["name"] = S.devName; d["mf"] = "Potahto"; d["mdl"] = "XIAO ESP32-S3"; d["sw"] = FW_VERSION;
  o["avty_t"] = String(s_root) + "/avail";
}
static void discPub(const char* comp, const char* obj, JsonDocument& d) {
  String t = String(C.haPrefix) + "/" + comp + "/" + s_id + "/" + obj + "/config";
  String p; serializeJson(d, p);
  esp_mqtt_client_publish(s_mq, t.c_str(), p.c_str(), 0, 1, 1);
}
static void discSensor(const char* obj, const char* name, const char* cls, const char* unit, const char* tpl, bool diag) {
  JsonDocument d;
  d["name"] = name; d["uniq_id"] = String(s_id) + "_" + obj;
  d["stat_t"] = String(s_root) + "/state"; d["val_tpl"] = tpl;
  if (cls) d["dev_cla"] = cls;
  if (unit) d["unit_of_meas"] = unit;
  d["stat_cla"] = "measurement";
  if (diag) d["ent_cat"] = "diagnostic";
  devBlock(d.as<JsonObject>());
  discPub("sensor", obj, d);
}
static void discBinary(const char* obj, const char* name, const char* cls) {
  JsonDocument d;
  d["name"] = name; d["uniq_id"] = String(s_id) + "_" + obj;
  d["stat_t"] = String(s_root) + "/" + obj; d["dev_cla"] = cls;
  devBlock(d.as<JsonObject>());
  discPub("binary_sensor", obj, d);
}
static void discTrigger(const char* event, const char* type, const char* subtype) {
  JsonDocument d;
  d["automation_type"] = "trigger";
  d["topic"] = String(s_root) + "/trigger";
  d["payload"] = event; d["type"] = type; d["subtype"] = subtype;
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(s_id); dev["name"] = S.devName;
  char obj[40]; strlcpy(obj, event, sizeof(obj)); for (char* p = obj; *p; p++) if (*p == '/') *p = '_';
  discPub("device_automation", obj, d);
}
static void announce() {
  pub("avail", "online", true, 1);
  char t[96]; snprintf(t, sizeof(t), "%s/cmd/#", s_root);
  esp_mqtt_client_subscribe_single(s_mq, t, 0);
  if (C.ha) {
    discSensor("battery", "Battery", "battery", "%", "{{ value_json.battery }}", false);
    discSensor("voltage", "Battery voltage", "voltage", "V", "{{ value_json.voltage }}", true);
    discSensor("temperature", "Temperature", "temperature", "\xC2\xB0""C", "{{ value_json.temp }}", true);
    discSensor("rssi", "WiFi signal", "signal_strength", "dBm", "{{ value_json.rssi }}", true);
    discBinary("tripwire", "Tripwire", "motion");
    discBinary("presence", "Desk presence", "occupancy");
    { JsonDocument d; d["name"] = "Ring"; d["uniq_id"] = String(s_id) + "_ring"; d["cmd_t"] = String(s_root) + "/cmd/ring";
      d["icon"] = "mdi:bell-ring"; devBlock(d.as<JsonObject>()); discPub("button", "ring", d); }
    { JsonDocument d; d["name"] = "Message"; d["uniq_id"] = String(s_id) + "_message"; d["cmd_t"] = String(s_root) + "/cmd/message";
      d["max"] = 60; d["icon"] = "mdi:message-text"; devBlock(d.as<JsonObject>()); discPub("text", "message", d); }
    static const char* SUB[5] = { "button_1", "button_2", "button_3", "button_4", "button_5" };
    char ev[32];
    for (int b = 0; b < 5; b++) {
      snprintf(ev, sizeof(ev), "button/%s_short", BTN_NAME[b]); discTrigger(ev, "button_short_press", SUB[b]);
      if (b < 4) { snprintf(ev, sizeof(ev), "button/%s_long", BTN_NAME[b]); discTrigger(ev, "button_long_press", SUB[b]); }
    }
    for (const char* e : SYS_EVENTS) {
      char type[16], sub[16]; const char* sl = strchr(e, '/');
      strlcpy(type, e, min((size_t)(sl - e + 1), sizeof(type))); strlcpy(sub, sl + 1, sizeof(sub));
      discTrigger(e, type, sub);
    }
  }
  pub("tripwire", s_trip ? "ON" : "OFF", true);
  pub("presence", s_presence ? "ON" : "OFF", true);
  s_lastState = 0;                                      // publish state right away
  if (!s_bootSent) { s_bootSent = true; iotEvent("boot", "online"); }
}
static void publishState() {
  Vec3f a, g; float t = NAN;
  if (mpuPresent()) mpuRead(a, g, t);
  char p[160];
  snprintf(p, sizeof(p), "{\"battery\":%u,\"voltage\":%.2f,\"temp\":%.1f,\"rssi\":%d,\"uptime\":%lu}",
           battPercent(), battVoltage(), isnan(t) ? 0.0f : t, netRssi(), (unsigned long)(millis() / 1000));
  pub("state", p, true);
}
static void mqttStop() {
  if (!s_mq) return;
  esp_mqtt_client_stop(s_mq); esp_mqtt_client_destroy(s_mq);
  s_mq = nullptr; s_mqConnected = false;
}
static void mqttStart() {
  bool tls = C.port == 8883;
  snprintf(s_uri, sizeof(s_uri), "%s://%s:%u", tls ? "mqtts" : "mqtt", C.host, C.port);
  snprintf(s_lwt, sizeof(s_lwt), "%s/avail", s_root);
  esp_mqtt_client_config_t cfg = {};
  cfg.broker.address.uri = s_uri;
  if (tls) cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.credentials.client_id = s_id;
  if (C.user[0]) cfg.credentials.username = C.user;
  if (C.pass[0]) cfg.credentials.authentication.password = C.pass;
  cfg.session.last_will.topic = s_lwt;
  cfg.session.last_will.msg = "offline";
  cfg.session.last_will.qos = 1;
  cfg.session.last_will.retain = 1;
  cfg.session.keepalive = 60;
  cfg.network.reconnect_timeout_ms = 10000;
  cfg.network.timeout_ms = 5000;
  cfg.buffer.size = 1536;
  s_mq = esp_mqtt_client_init(&cfg);
  if (!s_mq) { s_mqState = MQ_ERROR; return; }
  esp_mqtt_client_register_event(s_mq, MQTT_EVENT_ANY, mqttHandler, nullptr);
  esp_mqtt_client_start(s_mq);
  s_mqState = MQ_CONNECTING;
}
static void mqttRestart() { mqttStop(); s_mqState = MQ_OFF; }

static void handleInbound(const char* topic, const char* data) {
  const char* cmd = strstr(topic, "/cmd/");
  if (!cmd) return;
  cmd += 5;
  if (!strcmp(cmd, "ring"))         ringStart("Find me!", "from Home Assistant", 20000, 255);
  else if (!strcmp(cmd, "message")) notifyPush("Home Assistant", data, "ha");
  else if (!strcmp(cmd, "toast"))   { toast(data); fbToastCue(); }
  else if (!strcmp(cmd, "notify")) {
    JsonDocument d;
    if (!deserializeJson(d, data) && d.is<JsonObject>()) notifyPush(d["title"] | "Home Assistant", d["text"] | "", "mqtt");
    else notifyPush("Home Assistant", data, "mqtt");
  }
}

IotMqttState iotMqttState() { return s_mqState; }
const char*  iotMqttStateName() {
  switch (s_mqState) {
    case MQ_OFF: return "Off"; case MQ_WAIT_WIFI: return "Waiting for WiFi";
    case MQ_CONNECTING: return "Connecting"; case MQ_CONNECTED: return "Connected";
    case MQ_ERROR: return "Can't reach broker";
  }
  return "?";
}

// ---- events
void iotEvent(const char* type, const char* sub) {
  char ev[32]; snprintf(ev, sizeof(ev), "%s/%s", type, sub);
  strlcpy(s_lastEvent, ev, sizeof(s_lastEvent));
  if (s_mqConnected) {
    pub("trigger", ev, false);
    char p[96]; snprintf(p, sizeof(p), "{\"event\":\"%s\",\"battery\":%u}", ev, battPercent());
    pub("event", p, false);
  }
  for (int i = 0; i < C.nRules; i++) if (!strcmp(C.rules[i].ev, ev)) fireHook(C.rules[i].hook, ev);
}
void iotState(const char* what, bool on) {
  if (!strcmp(what, "tripwire")) s_trip = on; else if (!strcmp(what, "presence")) s_presence = on;
  pub(what, on ? "ON" : "OFF", true);
}
void iotButton(int idx, bool hold) {
  if (idx < 0 || idx > 4 || (hold && idx > 3)) return;
  int k = hold ? 5 + idx : idx;
  char sub[16]; snprintf(sub, sizeof(sub), "%s_%s", BTN_NAME[idx], hold ? "long" : "short");
  iotEvent("button", sub);
  char ev[32]; snprintf(ev, sizeof(ev), "button/%s", sub);
  fireHook(C.btnHook[k], ev);
}

// ---- protocol
static bool cIotGet(JsonObjectConst, JsonObject r, ProtoCtx&) {
  cfgToJson(r["cfg"].to<JsonObject>(), false);
  JsonArray ev = r["events"].to<JsonArray>();
  for (const char* e : SYS_EVENTS) ev.add(e);
  for (int b = 0; b < 5; b++) { ev.add(String("button/") + BTN_NAME[b] + "_short"); if (b < 4) ev.add(String("button/") + BTN_NAME[b] + "_long"); }
  r["devId"] = s_id; r["root"] = s_root; r["stored"] = storeOk();   // NB: "id" is the request id
  r["limits"]["hooks"] = IOT_HOOKS; r["limits"]["rules"] = IOT_RULES;
  return true;
}
static bool cIotSet(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  JsonObjectConst j = q["cfg"];
  if (j.isNull()) { r["err"] = "missing cfg"; return false; }
  for (JsonObjectConst h : j["hooks"].as<JsonArrayConst>()) {
    const char* u = h["url"] | "";
    if (u[0] && strncmp(u, "http://", 7) && strncmp(u, "https://", 8)) { r["err"] = "webhook URLs must start with http:// or https://"; return false; }
    if (strlen(u) >= sizeof(Hook::url)) { r["err"] = "webhook URL too long (max 199)"; return false; }
    if (strlen(h["body"] | "") >= sizeof(Hook::body)) { r["err"] = "webhook body too long (max 299)"; return false; }
  }
  if (strlen(j["mqtt"]["host"] | "") >= sizeof(C.host)) { r["err"] = "broker host too long"; return false; }
  cfgFromJson(j, true);
  if (!cfgSave()) { r["err"] = "could not save (no file storage?)"; return false; }
  mqttRestart();
  return true;
}
static bool cIotTest(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  int i = q["hook"] | -1;
  if (!fireHook(i, "test/manual")) { r["err"] = "no such webhook"; return false; }
  return true;
}
static bool cIotStatus(JsonObjectConst, JsonObject r, ProtoCtx&) {
  r["mqtt"] = iotMqttStateName(); r["mqttOn"] = iotMqttEnabled();
  r["lastEvent"] = s_lastEvent; r["lastHook"] = s_lastHook; r["lastCode"] = s_lastCode;
  r["pending"] = (uint32_t)(s_hookQueued - s_hookDone);
  return true;
}
static bool cIotEvent(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  const char* t = q["type"] | ""; const char* s = q["sub"] | "";
  if (!t[0] || !s[0]) { r["err"] = "need type and sub"; return false; }
  iotEvent(t, s); return true;
}

// ---- lifecycle
void iotBegin() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(s_id, sizeof(s_id), "potahto_%02x%02x%02x", mac[3], mac[4], mac[5]);
  cfgLoad();
  snprintf(s_root, sizeof(s_root), "%s/%s", C.base, s_id);
  s_hq = xQueueCreate(4, sizeof(HookReq));
  xTaskCreate(hookTask, "webhook", 8192, nullptr, 1, nullptr);
  protoRegister("iot.get",    cIotGet);
  protoRegister("iot.set",    cIotSet);
  protoRegister("iot.test",   cIotTest);
  protoRegister("iot.status", cIotStatus);
  protoRegister("iot.event",  cIotEvent);
}

void iotLoop() {
  // webhooks hold WiFi only while requests are outstanding
  bool hooksBusy = s_hookQueued != s_hookDone;
  if (hooksBusy && !s_hookNet) { netAcquire(); s_hookNet = true; }
  if (!hooksBusy && s_hookNet) { netRelease(); s_hookNet = false; }

  // inbound MQTT commands -> main thread
  while (s_inHead != s_inTail) {
    Inbound m;
    portENTER_CRITICAL(&s_inMux); m = s_in[s_inHead]; s_inHead = (s_inHead + 1) % 4; portEXIT_CRITICAL(&s_inMux);
    handleInbound(m.topic, m.data);
  }

  snprintf(s_root, sizeof(s_root), "%s/%s", C.base, s_id);
  bool want = iotMqttEnabled();
  if (want && !s_mqNet)  { netAcquire(); s_mqNet = true; }
  if (!want && s_mqNet)  { netRelease(); s_mqNet = false; }
  if (!want) { if (s_mq) mqttStop(); s_mqState = MQ_OFF; return; }
  if (!netOnline()) { if (s_mq) mqttStop(); s_mqState = MQ_WAIT_WIFI; return; }
  if (!s_mq) mqttStart();
  if (s_mqConnected) {
    s_mqState = MQ_CONNECTED;
    if (s_announce) { s_announce = false; announce(); }
    if (millis() - s_lastState > 60000) { s_lastState = millis(); publishState(); }
  } else s_mqState = s_mqError ? MQ_ERROR : MQ_CONNECTING;
}
