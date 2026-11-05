// Host implementations of the ESP-IDF pieces used by Iot.cpp / Notify.cpp,
//
// so the REAL firmware code runs in the simulator.
#include <Arduino.h>
#include "mqtt_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <string>
#include <chrono>

// ---- FreeRTOS -> std::thread
struct HostQueue { std::mutex m; std::condition_variable cv; std::deque<std::vector<uint8_t>> q; size_t sz, cap; };
QueueHandle_t xQueueCreate(int len, size_t sz) { auto* h = new HostQueue; h->sz = sz; h->cap = len; return h; }
BaseType_t xQueueSend(QueueHandle_t h, const void* item, TickType_t) {
  std::lock_guard<std::mutex> l(h->m);
  if (h->q.size() >= h->cap) return pdFALSE;
  h->q.emplace_back((const uint8_t*)item, (const uint8_t*)item + h->sz); h->cv.notify_one(); return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t h, void* item, TickType_t) {
  std::unique_lock<std::mutex> l(h->m);
  h->cv.wait(l, [&] { return !h->q.empty(); });
  memcpy(item, h->q.front().data(), h->sz); h->q.pop_front(); return pdTRUE;
}
BaseType_t xTaskCreate(TaskFunction_t fn, const char*, uint32_t, void* arg, int, void*) { std::thread(fn, arg).detach(); return pdTRUE; }
void vTaskDelay(TickType_t) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

// ---- HTTP client: records requests
std::mutex g_httpLogM;
std::vector<std::string> g_httpLog;
struct esp_http_client { std::string url, body, ctype; int method; };
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* c) { auto* h = new esp_http_client; h->url = c->url; h->method = c->method; return h; }
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h, const char* k, const char* v) { if (!strcmp(k, "Content-Type")) h->ctype = v; return 0; }
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t h, const char* d, int n) { h->body.assign(d, n); return 0; }
esp_err_t esp_http_client_perform(esp_http_client_handle_t h) {
  static const char* M[] = { "GET", "POST", "PUT" };
  std::lock_guard<std::mutex> l(g_httpLogM);
  g_httpLog.push_back(std::string(M[h->method]) + " " + h->url + " [" + h->ctype + "] " + h->body);
  return 0;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h) { return h->url.find("fail") != std::string::npos ? 500 : 200; }
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) { delete h; return 0; }
esp_err_t esp_crt_bundle_attach(void*) { return 0; }

// ---- MQTT client: connects instantly, records publishes
std::vector<std::pair<std::string, std::string>> g_mqttPub;
std::string g_mqttUri, g_mqttLwt;
struct esp_mqtt_client { esp_event_handler_t h = nullptr; };
static esp_mqtt_client* s_cli = nullptr;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t* c) {
  g_mqttUri = c->broker.address.uri; g_mqttLwt = std::string(c->session.last_will.topic) + "=" + c->session.last_will.msg;
  s_cli = new esp_mqtt_client; return s_cli;
}
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t c, esp_mqtt_event_id_t, esp_event_handler_t h, void*) { c->h = h; return 0; }
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t c) { esp_mqtt_event_t e = {}; e.event_id = MQTT_EVENT_CONNECTED; c->h(nullptr, "mqtt", MQTT_EVENT_CONNECTED, &e); return 0; }
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t) { return 0; }
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t c) { if (c == s_cli) s_cli = nullptr; delete c; return 0; }
int esp_mqtt_client_publish(esp_mqtt_client_handle_t, const char* t, const char* d, int, int, int) { g_mqttPub.push_back({ t, d }); return 1; }
int esp_mqtt_client_subscribe_single(esp_mqtt_client_handle_t, const char* t, int) { g_mqttPub.push_back({ std::string("SUBSCRIBE ") + t, "" }); return 1; }
void hostMqttInject(const char* topic, const char* data) {
  if (!s_cli || !s_cli->h) return;
  esp_mqtt_event_t e = {}; e.event_id = MQTT_EVENT_DATA;
  e.topic = (char*)topic; e.topic_len = strlen(topic); e.data = (char*)data; e.data_len = e.total_data_len = strlen(data);
  s_cli->h(nullptr, "mqtt", MQTT_EVENT_DATA, &e);
}
// ---- clock
extern "C" int potahto_fake_settimeofday(const void*, const void*) { return 0; }   // never touch the host clock
