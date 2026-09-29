// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900.h"
typedef struct {
    ax900_ap_t ap;
    bool enterprise, allow_unverified_server, association_test, skip_save;
    char password[129], username[129], server_name[254];
    char ca_pem[8193];
} ax_connect_request_t;
