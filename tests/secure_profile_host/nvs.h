#pragma once
#include "../profile_host/nvs.h"
#define ESP_ERR_NVS_NOT_INITIALIZED 0x1101
typedef struct {int unused;} nvs_stats_t;
esp_err_t nvs_get_stats(const char *,nvs_stats_t *);
esp_err_t nvs_open_from_partition(const char *,const char *,int,nvs_handle_t *);
