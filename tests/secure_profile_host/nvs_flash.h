#pragma once
#include "esp_err.h"
#include "esp_partition.h"
#include <stdint.h>
typedef struct {uint8_t eky[32],tky[32];} nvs_sec_cfg_t;
esp_err_t nvs_flash_read_security_cfg(const esp_partition_t *,nvs_sec_cfg_t *);
esp_err_t nvs_flash_secure_init_partition(const char *,nvs_sec_cfg_t *);
esp_err_t nvs_flash_init(void);
