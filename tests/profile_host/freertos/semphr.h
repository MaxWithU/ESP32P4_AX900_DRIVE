#pragma once
#define portMAX_DELAY 0xffffffff
#define xSemaphoreCreateMutex() ((void *)1)
#define xSemaphoreTake(s,t) ((void)(s))
#define xSemaphoreGive(s) ((void)(s))
typedef void *SemaphoreHandle_t;
