#pragma once
#include <stdbool.h>
typedef struct {bool encrypted;} esp_partition_t;
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS 4
const esp_partition_t *esp_partition_find_first(int,int,const char *);
