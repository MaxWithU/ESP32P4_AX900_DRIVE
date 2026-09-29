#pragma once
#include <stdint.h>
typedef void *TaskHandle_t;
typedef unsigned TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(n) (n)
static inline int xTaskCreate(void (*f)(void *),const char *n,unsigned s,void *a,unsigned p,TaskHandle_t *h){(void)f;(void)n;(void)s;(void)a;(void)p;*h=(void *)1;return 1;}
static inline void vTaskDelete(void *p){(void)p;}
static inline unsigned ulTaskNotifyTake(int clear,unsigned timeout){(void)clear;(void)timeout;return 0;}
static inline unsigned xTaskGetTickCount(void){return 0;}
static inline void vTaskDelayUntil(unsigned *last,unsigned interval){*last+=interval;}
static inline void xTaskNotifyGive(void *task){(void)task;}
