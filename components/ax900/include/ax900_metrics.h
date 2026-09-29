// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    AX900_TX_BYTES, AX900_RX_BYTES,
    AX900_TX_QUEUE_FULL, AX900_TX_POOL_EMPTY, AX900_TX_STALE, AX900_TX_USB_FAILED,
    AX900_RX_QUEUE_FULL, AX900_RX_POOL_EMPTY, AX900_RX_STALE, AX900_RX_MALFORMED,
    AX900_RX_UNAUTHORIZED, AX900_EAPOL_RX,
    AX900_TX_QUEUE_PEAK, AX900_RX_QUEUE_PEAK, AX900_POOL_USED, AX900_POOL_PEAK,
    AX900_USB_INFLIGHT, AX900_USB_INFLIGHT_PEAK, AX900_EVENT_DROPPED,
    AX900_METRIC_COUNT
} ax900_metric_t;
typedef struct { uint64_t value[AX900_METRIC_COUNT]; } ax900_metrics_t;
// Monotonic counters since boot, except POOL_USED and USB_INFLIGHT gauges.
void ax900_get_metrics(ax900_metrics_t *out);
#ifdef __cplusplus
}
#endif
