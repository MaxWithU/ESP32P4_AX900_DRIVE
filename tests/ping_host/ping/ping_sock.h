#pragma once
#include "lwip/test_lwip.h"
typedef void *esp_ping_handle_t;
typedef struct {void *cb_args;void (*on_ping_success)(esp_ping_handle_t,void *);void (*on_ping_timeout)(esp_ping_handle_t,void *);void (*on_ping_end)(esp_ping_handle_t,void *);} esp_ping_callbacks_t;
typedef struct {uint32_t count,interval_ms,timeout_ms,interface,task_stack_size,task_prio,data_size;uint8_t tos,ttl;ip_addr_t target_addr;} esp_ping_config_t;
typedef enum {ESP_PING_PROF_SEQNO,ESP_PING_PROF_TOS,ESP_PING_PROF_TTL,ESP_PING_PROF_REQUEST,ESP_PING_PROF_REPLY,ESP_PING_PROF_IPADDR,ESP_PING_PROF_SIZE,ESP_PING_PROF_TIMEGAP,ESP_PING_PROF_DURATION} esp_ping_profile_t;
