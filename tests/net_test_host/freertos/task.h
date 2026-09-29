#pragma once
#include <stdint.h>
typedef int BaseType_t;
#define pdMS_TO_TICKS(n) (n)
#define pdPASS 1
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,void *);
void vTaskDelete(void *);
void vTaskDelay(unsigned);
