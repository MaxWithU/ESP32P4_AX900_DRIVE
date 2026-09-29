#pragma once
#include <stdint.h>
#define pdMS_TO_TICKS(n) (n)
#define pdPASS 1
static inline int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle) {
    (void)fn;(void)name;(void)stack;(void)arg;(void)priority;(void)handle;return pdPASS;
}
