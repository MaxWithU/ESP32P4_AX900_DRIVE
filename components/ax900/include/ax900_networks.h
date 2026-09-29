// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    ax900_ap_t preferred;
    uint8_t access_points;
    bool band_24, band_5;
} ax900_network_t;

// Compare raw SSIDs, not their printable display names. Keep different security
// policies separate; hidden SSIDs cannot identify an ESS and stay per BSSID.
static inline bool ax900_same_network(const ax900_ap_t *a, const ax900_ap_t *b) {
    if (!a->ssid_len || !b->ssid_len)
        return !memcmp(a->bssid, b->bssid, 6);
    if (a->ssid_len > 32 || a->ssid_len != b->ssid_len ||
        memcmp(a->raw_ssid, b->raw_ssid, a->ssid_len) ||
        a->secured != b->secured || a->enterprise != b->enterprise ||
        a->wpa2_psk != b->wpa2_psk || a->sae != b->sae ||
        a->pmf_required != b->pmf_required) return false;
    if (a->secured && !a->wpa2_psk && !a->enterprise && !a->sae)
        return a->rsn_len <= sizeof(a->rsn) && a->rsn_len == b->rsn_len &&
               !memcmp(a->rsn, b->rsn, a->rsn_len);
    return true;
}

static inline bool ax900_prefer_ap(const ax900_ap_t *a, const ax900_ap_t *b) {
    if ((a->frequency > 5000) != (b->frequency > 5000)) return a->frequency > 5000;
    if (a->rssi != b->rssi) return a->rssi > b->rssi;
    return memcmp(a->bssid, b->bssid, 6) < 0;
}

// Retains the raw scan separately for recovery/diagnostics. No allocation.
static inline size_t ax900_group_networks(const ax900_ap_t *aps, size_t count,
                                         ax900_network_t *out, size_t capacity) {
    if (!aps || !out) return 0;
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t j = 0;
        while (j < used && !ax900_same_network(&aps[i], &out[j].preferred)) ++j;
        if (j == used) {
            if (used == capacity) continue;
            memset(&out[used], 0, sizeof(out[used]));
            out[used++].preferred = aps[i];
        }
        if (out[j].access_points < UINT8_MAX) ++out[j].access_points;
        if (aps[i].frequency > 5000) out[j].band_5 = true;
        else out[j].band_24 = true;
        if (ax900_prefer_ap(&aps[i], &out[j].preferred)) out[j].preferred = aps[i];
    }
    // Stable ordering: preferred band and RSSI, then BSSID as deterministic tie-break.
    for (size_t i = 1; i < used; ++i) {
        ax900_network_t item = out[i];
        size_t j = i;
        while (j && ax900_prefer_ap(&item.preferred, &out[j-1].preferred)) {
            out[j] = out[j-1]; --j;
        }
        out[j] = item;
    }
    return used;
}
#ifdef __cplusplus
}
#endif
