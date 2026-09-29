#pragma once
#include "../probe_host/esp_netif.h"
#define ESP_IPADDR_TYPE_V4 0
#define ESP_NETIF_DNS_MAIN 0
typedef struct {ip_addr_t ip;} esp_netif_dns_info_t;
esp_err_t esp_netif_get_dns_info(esp_netif_t *,int,esp_netif_dns_info_t *);
