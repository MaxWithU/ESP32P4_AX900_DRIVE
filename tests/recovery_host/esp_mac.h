#pragma once
#include <stdint.h>
#include "esp_err.h"
#define ESP_MAC_EFUSE_FACTORY 0
static inline esp_err_t esp_read_mac(uint8_t mac[6],int type){(void)type;for(unsigned i=0;i<6;i++)mac[i]=i;return ESP_OK;}
static inline esp_err_t esp_derive_local_mac(uint8_t mac[6],const uint8_t base[6]){for(unsigned i=0;i<6;i++)mac[i]=base[i];mac[0]=2;return ESP_OK;}
