// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900_credentials.h"
esp_err_t ax_profile_init(void);
esp_err_t ax_profile_save(const ax_connect_request_t *request);
esp_err_t ax_profile_forget(void);
// Returns a sanitized heap request for a currently scanned, security-matching AP.
ax_connect_request_t *ax_profile_find(const ax900_ap_t *aps, size_t count);
unsigned ax_profile_count(void);
