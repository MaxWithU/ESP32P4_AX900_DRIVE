#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
// The application owns USB host installation and board power.
esp_err_t ax900_start(void);
#define AX900_MAX_APS 64
typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint16_t frequency;
    int8_t rssi;
    bool secured;
} ax900_ap_t;
typedef struct {
    char status[96];
    bool present, ready, supports_5ghz, scanning;
    uint32_t scan_generation;
    size_t ap_count;
    ax900_ap_t aps[AX900_MAX_APS];
} ax900_status_t;
void ax900_get_status(ax900_status_t *out);
esp_err_t ax900_request_scan(void);
#ifdef __cplusplus
}
#endif
