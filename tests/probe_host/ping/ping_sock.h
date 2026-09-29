#pragma once
#include "esp_netif.h"
typedef void *esp_ping_handle_t;
typedef struct {
    void *cb_args;
    void (*on_ping_success)(esp_ping_handle_t,void *);
    void (*on_ping_timeout)(esp_ping_handle_t,void *);
    void (*on_ping_end)(esp_ping_handle_t,void *);
} esp_ping_callbacks_t;
typedef struct {uint32_t count,interval_ms,timeout_ms,interface;ip_addr_t target_addr;} esp_ping_config_t;
#define ESP_PING_DEFAULT_CONFIG() {0}
typedef enum {ESP_PING_PROF_REQUEST,ESP_PING_PROF_REPLY,ESP_PING_PROF_TIMEGAP} esp_ping_profile_t;
esp_err_t esp_ping_new_session(const esp_ping_config_t *,const esp_ping_callbacks_t *,esp_ping_handle_t *);
esp_err_t esp_ping_start(esp_ping_handle_t);
esp_err_t esp_ping_stop(esp_ping_handle_t);
esp_err_t esp_ping_delete_session(esp_ping_handle_t);
esp_err_t esp_ping_get_profile(esp_ping_handle_t,esp_ping_profile_t,void *,size_t);
