// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sdkconfig.h"
#include "esp_netif.h"

// Without this IDF option, get_dns_info ignores the client netif and reads the
// global resolver. Never label another interface's DNS as AX900 DHCP data.
static inline esp_err_t ax_netif_get_dns(esp_netif_t *netif,esp_netif_dns_info_t *dns){
#if CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF
    return esp_netif_get_dns_info(netif,ESP_NETIF_DNS_MAIN,dns);
#else
    (void)netif;(void)dns;return ESP_ERR_NOT_SUPPORTED;
#endif
}
