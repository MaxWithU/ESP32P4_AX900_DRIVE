// SPDX-License-Identifier: Apache-2.0
// Reuse the selected IDF's ICMP implementation; change only task stack placement.
// The official display + PEAP can fragment internal RAM while PSRAM remains free.
#include "ax900_ping.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM && CONFIG_SPIRAM
#undef xTaskCreate
#define xTaskCreate(fn,name,size,arg,prio,out) \
    xTaskCreateWithCaps(fn,name,size,arg,prio,out,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define vTaskDelete(task) vTaskDeleteWithCaps(task)
#endif
// Include from the SDK, avoiding an unmaintained fork of its packet handling.
#include "apps/ping/ping_sock.c"
