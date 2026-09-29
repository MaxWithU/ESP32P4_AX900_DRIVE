// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {char country[3];bool allow_dfs;} ax900_radio_config_t;
typedef struct {
    uint32_t firmware_version,firmware_features;
    bool compact_feature_map,firmware_pmf;
    bool open,wpa2_personal,peap_mschapv2,wpa3_sae,pmf,seamless_roaming;
} ax900_capabilities_t;
// Configure before start, or after a completed stop. Uses only channels included
// in the bundled D80 radio/power table; it never unlocks arbitrary frequencies.
esp_err_t ax900_configure_radio(const ax900_radio_config_t *config);
void ax900_get_radio_config(ax900_radio_config_t *config);
void ax900_get_capabilities(ax900_capabilities_t *capabilities);
#ifdef __cplusplus
}
#endif
