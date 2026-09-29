#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct {uint32_t addr;} ip4_addr_t;
typedef struct {int type;union {ip4_addr_t ip4;} u_addr;} ip_addr_t;
typedef struct {ip4_addr_t ip,gw,netmask;} esp_netif_ip_info_t;
typedef struct {int unused;} esp_netif_t;
#define IPADDR_TYPE_V4 0
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(p) (unsigned)((p)->addr&255),(unsigned)(((p)->addr>>8)&255),(unsigned)(((p)->addr>>16)&255),(unsigned)((p)->addr>>24)
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif,esp_netif_ip_info_t *ip);
int esp_netif_get_netif_impl_index(esp_netif_t *netif);
