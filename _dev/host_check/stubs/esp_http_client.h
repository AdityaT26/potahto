#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef enum { HTTP_METHOD_GET = 0, HTTP_METHOD_POST, HTTP_METHOD_PUT } esp_http_client_method_t;
typedef struct { const char* url; esp_http_client_method_t method; int timeout_ms; esp_err_t (*crt_bundle_attach)(void*); const char* user_agent; } esp_http_client_config_t;
typedef struct esp_http_client* esp_http_client_handle_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char*, const char*);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t, const char*, int);
esp_err_t esp_http_client_perform(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
