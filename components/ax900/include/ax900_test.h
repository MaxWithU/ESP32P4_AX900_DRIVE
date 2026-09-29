// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_err.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { AX900_TEST_DNS, AX900_TEST_TCP, AX900_TEST_HTTP,
    AX900_TEST_UPLOAD, AX900_TEST_DOWNLOAD, AX900_TEST_UDP } ax900_test_kind_t;
typedef enum { AX900_TEST_IDLE, AX900_TEST_RUNNING, AX900_TEST_PASSED,
    AX900_TEST_FAILED, AX900_TEST_CANCELLED, AX900_TEST_STALE } ax900_test_state_t;
typedef enum { AX900_TEST_STAGE_NONE, AX900_TEST_STAGE_INTERFACE, AX900_TEST_STAGE_DNS,
    AX900_TEST_STAGE_SOCKET, AX900_TEST_STAGE_CONNECT, AX900_TEST_STAGE_TRANSFER,
    AX900_TEST_STAGE_RESPONSE } ax900_test_stage_t;
typedef struct {
    ax900_test_kind_t kind;
    char host[254];             // IPv4 literal or DNS name; no URL or credentials.
    char dns_ipv4[16];          // Empty = AX900 DHCP DNS. Queries never use another interface.
    char path[128];             // HTTP path, starting with '/'.
    uint16_t port;              // TCP/HTTP/benchmark peer port, DNS always uses 53.
    uint32_t timeout_ms;        // Total test deadline, 1000..60000 ms.
    uint32_t bytes;             // Benchmark payload, 1024..8388608 bytes.
} ax900_test_config_t;
typedef struct {
    ax900_test_state_t state;
    ax900_test_kind_t kind;
    ax900_test_stage_t stage;
    esp_err_t error;
    int socket_error, http_status;
    uint32_t connection_id, elapsed_ms, bytes, kilobits_per_second;
    uint32_t sent, received, min_ms, max_ms, average_ms;
    uint32_t internal_before, internal_after, internal_minimum;
} ax900_test_result_t;
// Runs only after an authenticated AX900 DHCP lease. Targets are explicit,
// RAM-only, bounded, and never logged. Upload/download require tools/benchmark_peer.py.
esp_err_t ax900_test_start(const ax900_test_config_t *config);
void ax900_test_cancel(void);
void ax900_test_get_result(ax900_test_result_t *result);
#ifdef __cplusplus
}
#endif
