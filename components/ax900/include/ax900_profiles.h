// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AX900_PROFILE_LIMIT 4
typedef struct {
    ax900_ap_t network;         // Only SSID and security fields are populated.
    bool auto_connect;
    bool verifies_server;
} ax900_saved_network_t;
// Metadata only: passwords, identities, certificates and server names never leave NVS.
size_t ax900_list_saved(ax900_saved_network_t *out,size_t capacity);
esp_err_t ax900_forget_network(const ax900_ap_t *network);
esp_err_t ax900_set_auto_connect(const ax900_ap_t *network,bool enabled);
// True only for the protected, preprovisioned encrypted profile partition.
bool ax900_profiles_encrypted(void);
#ifdef __cplusplus
}
#endif
