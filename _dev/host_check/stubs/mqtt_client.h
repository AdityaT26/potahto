#pragma once
// host stub of ESP-IDF esp-mqtt: records publishes for tests
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
typedef const char* esp_event_base_t;
typedef void (*esp_event_handler_t)(void*, esp_event_base_t, int32_t, void*);
typedef struct esp_mqtt_client* esp_mqtt_client_handle_t;
typedef enum { MQTT_EVENT_ANY = -1, MQTT_EVENT_ERROR = 0, MQTT_EVENT_CONNECTED, MQTT_EVENT_DISCONNECTED, MQTT_EVENT_SUBSCRIBED,
  MQTT_EVENT_UNSUBSCRIBED, MQTT_EVENT_PUBLISHED, MQTT_EVENT_DATA } esp_mqtt_event_id_t;
typedef struct { esp_mqtt_event_id_t event_id; esp_mqtt_client_handle_t client; char* data; int data_len; int total_data_len;
  int current_data_offset; char* topic; int topic_len; } esp_mqtt_event_t;
typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;
typedef struct esp_mqtt_client_config_t {
  struct { struct { const char* uri; } address; struct { esp_err_t (*crt_bundle_attach)(void*); } verification; } broker;
  struct { const char* username; const char* client_id; struct { const char* password; } authentication; } credentials;
  struct { struct { const char* topic; const char* msg; int qos; int retain; } last_will; int keepalive; } session;
  struct { int reconnect_timeout_ms; int timeout_ms; } network;
  struct { int size; } buffer;
} esp_mqtt_client_config_t;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t*);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t);
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t, const char* topic, const char* data, int len, int qos, int retain);
int esp_mqtt_client_subscribe_single(esp_mqtt_client_handle_t, const char* topic, int qos);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t, esp_mqtt_event_id_t, esp_event_handler_t, void*);
