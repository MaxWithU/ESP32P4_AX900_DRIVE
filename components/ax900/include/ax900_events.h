// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_event.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
ESP_EVENT_DECLARE_BASE(AX900_EVENTS);
enum { AX900_EVENT_STATE_CHANGED = 1 };
enum {
    AX900_STATE_PRESENT=1, AX900_STATE_READY=2, AX900_STATE_SCANNING=4,
    AX900_STATE_CONNECTING=8, AX900_STATE_ASSOCIATED=16,
    AX900_STATE_AUTHENTICATED=32, AX900_STATE_HAS_IP=64
};
// No SSID, addresses or credentials in events. Obtain a compact snapshot as needed.
// Posted on the default ESP event loop without blocking the USB worker.
typedef struct {uint32_t flags,connection_id,scan_generation;uint16_t reason;uint8_t lifecycle,saved_networks;} ax900_event_t;
#ifdef __cplusplus
}
#endif
