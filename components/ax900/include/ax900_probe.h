// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif

#define AX900_PROBE_COUNT 5
typedef enum {
    AX900_PROBE_IDLE,
    AX900_PROBE_RUNNING,
    AX900_PROBE_PASSED,
    AX900_PROBE_PARTIAL,
    AX900_PROBE_FAILED,
    AX900_PROBE_STALE,
} ax900_probe_state_t;

typedef struct {
    ax900_probe_state_t state;
    uint32_t connection_id;
    char gateway[16];
    uint32_t sent, received, elapsed_ms;
    uint32_t min_ms, max_ms, average_ms;
    // -1: pending, -2: timeout; zero is a valid measured RTT.
    int32_t rtt_ms[AX900_PROBE_COUNT];
    esp_err_t error;
} ax900_probe_result_t;

// Five asynchronous ICMP requests to the DHCP gateway, bound to AX900.
// A second request while running returns ESP_ERR_INVALID_STATE.
esp_err_t ax900_probe_start(void);
// Thread-safe snapshot. Results from a previous connection are marked STALE.
void ax900_probe_get_result(ax900_probe_result_t *out);
#ifdef __cplusplus
}
#endif
