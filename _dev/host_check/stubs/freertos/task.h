#pragma once
#include "freertos/FreeRTOS.h"
typedef void (*TaskFunction_t)(void*);
BaseType_t xTaskCreate(TaskFunction_t, const char*, uint32_t, void*, int, void*);
void vTaskDelay(TickType_t);
