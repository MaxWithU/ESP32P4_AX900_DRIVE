#pragma once
#include "../probe_host/esp_err.h"
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
static inline const char *esp_err_to_name(esp_err_t e){(void)e;return "synthetic error";}
