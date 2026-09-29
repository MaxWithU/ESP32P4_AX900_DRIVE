#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
typedef const char *esp_event_base_t;
#define ESP_EVENT_DECLARE_BASE(x) extern esp_event_base_t x
#define ESP_EVENT_DEFINE_BASE(x) esp_event_base_t x = #x
static inline esp_err_t esp_event_post(esp_event_base_t b,int32_t id,const void *p,size_t n,unsigned ticks){(void)b;(void)id;(void)p;(void)n;(void)ticks;return ESP_OK;}
static bool fake_default_event_loop;
static inline esp_err_t esp_event_loop_create_default(void){if(fake_default_event_loop)return ESP_ERR_INVALID_STATE;fake_default_event_loop=true;return ESP_OK;}
