// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900.h"
#include "ax900_test.h"
#ifdef __cplusplus
extern "C" {
#endif
// Stable categories, never inferred from human-readable status/log strings.
typedef enum {
    AX900_ISSUE_NONE, AX900_ISSUE_ASSOCIATION, AX900_ISSUE_AUTHENTICATION,
    AX900_ISSUE_AUTH_TIMEOUT, AX900_ISSUE_DHCP, AX900_ISSUE_CLOCK,
    AX900_ISSUE_TLS_CONFIG, AX900_ISSUE_CA, AX900_ISSUE_CERTIFICATE,
    AX900_ISSUE_CERT_DATE, AX900_ISSUE_CERT_NAME, AX900_ISSUE_TLS,
    AX900_ISSUE_MEMORY, AX900_ISSUE_CONNECTION
} ax900_issue_t;
typedef struct {const char *title;const char *action;} ax900_help_t;
ax900_issue_t ax900_get_connection_issue(void);
ax900_help_t ax900_issue_help(ax900_issue_t issue);
ax900_help_t ax900_connection_help(const ax900_link_status_t *link);
ax900_help_t ax900_request_help(esp_err_t error);
ax900_help_t ax900_test_help(const ax900_test_result_t *result,esp_err_t start_error);
#ifdef __cplusplus
}
#endif
