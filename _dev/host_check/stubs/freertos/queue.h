#pragma once
#include "freertos/FreeRTOS.h"
#include <stddef.h>
typedef struct HostQueue* QueueHandle_t;
QueueHandle_t xQueueCreate(int len, size_t itemSize);
BaseType_t xQueueSend(QueueHandle_t, const void* item, TickType_t);
BaseType_t xQueueReceive(QueueHandle_t, void* item, TickType_t);
