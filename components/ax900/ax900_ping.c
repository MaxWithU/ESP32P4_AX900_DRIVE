// SPDX-License-Identifier: Apache-2.0
// Maintained socket implementation derived from ESP-IDF v5.5.2 ping_sock.c.
// See NOTICE. Only public lwIP/IDF headers are used.
// The official display + PEAP can fragment internal RAM while PSRAM remains free.
#include "ax900_ping.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM && CONFIG_SPIRAM
#undef xTaskCreate
#define xTaskCreate(fn,name,size,arg,prio,out) \
    xTaskCreateWithCaps(fn,name,size,arg,prio,out,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define vTaskDelete(task) vTaskDeleteWithCaps(task)
#endif

/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>
#include "esp_timer.h"
#include <sys/time.h>
#include <net/if.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/mem.h"
#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "esp_check.h"

const static char *TAG = "ping_sock";

#define PING_TIME_DIFF_MS(_end, _start) ((uint32_t)(((_end).tv_sec - (_start).tv_sec) * 1000 + \
                                                    ((_end).tv_usec - (_start).tv_usec) / 1000))

#define PING_CHECK_START_TIMEOUT_MS (1000)

#define PING_FLAGS_INIT (1 << 0)
#define PING_FLAGS_START (1 << 1)

#define IP_ICMP_HDR_SIZE (68)   // Maximum IPv4 header (60) plus complete echo header (8).

typedef struct {
    int sock;
    struct sockaddr_storage target_addr;
    TaskHandle_t ping_task_hdl;
    struct icmp_echo_hdr *packet_hdr;
    ip_addr_t recv_addr;
    uint32_t recv_len;
    uint32_t icmp_pkt_size;
    uint32_t count;
    uint32_t transmitted;
    uint32_t received;
    uint32_t interval_ms;
    uint32_t timeout_ms;
    uint32_t elapsed_time_ms;
    uint32_t total_time_ms;
    uint8_t ttl;
    uint8_t tos;
    atomic_uint flags;
    void (*on_ping_success)(esp_ping_handle_t hdl, void *args);
    void (*on_ping_timeout)(esp_ping_handle_t hdl, void *args);
    void (*on_ping_end)(esp_ping_handle_t hdl, void *args);
    void *cb_args;
} esp_ping_t;

static esp_err_t esp_ping_send(esp_ping_t *ep)
{
    esp_err_t ret = ESP_OK;
    ep->packet_hdr->seqno++;
    /* generate checksum since "seqno" has changed */
    ep->packet_hdr->chksum = 0;
    if (ep->packet_hdr->type == ICMP_ECHO) {
        ep->packet_hdr->chksum = inet_chksum(ep->packet_hdr, ep->icmp_pkt_size);
    }

    ssize_t sent = sendto(ep->sock, ep->packet_hdr, ep->icmp_pkt_size, 0,
                      (struct sockaddr *)&ep->target_addr, sizeof(ep->target_addr));

    if (sent != (ssize_t)ep->icmp_pkt_size) {
        int opt_val;
        socklen_t opt_len = sizeof(opt_val);
        getsockopt(ep->sock, SOL_SOCKET, SO_ERROR, &opt_val, &opt_len);
        ESP_LOGE(TAG, "send error=%d", opt_val);
        ret = ESP_FAIL;
    } else {
        ep->transmitted++;
    }
    return ret;
}

static int esp_ping_receive(esp_ping_t *ep)
{
    char buf[IP_ICMP_HDR_SIZE];
    int len;
    struct sockaddr_storage from;
    ip_addr_t recv_addr;
    const int64_t deadline = esp_timer_get_time() + (int64_t)ep->timeout_ms * 1000;

    while ((ep->flags & (PING_FLAGS_INIT | PING_FLAGS_START)) == (PING_FLAGS_INIT | PING_FLAGS_START)) {
        // A stream of unrelated ICMP must not restart the timeout on each recv.
        int64_t left = deadline - esp_timer_get_time();
        if (left <= 0) break;
        // Short slices also bound cancellation latency when no packets arrive.
        if (left > 100000) left = 100000;
        struct timeval timeout = {.tv_sec = 0, .tv_usec = left};
        if (setsockopt(ep->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) return -1;
        socklen_t fromlen = sizeof(from);
        len = recvfrom(ep->sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
        if (esp_timer_get_time() >= deadline ||
            (ep->flags & (PING_FLAGS_INIT | PING_FLAGS_START)) != (PING_FLAGS_INIT | PING_FLAGS_START)) break;
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            return -1;
        }
        if (!len) continue;
        uint16_t data_head;
        ip_addr_copy(recv_addr, *IP_ADDR_ANY);
#if CONFIG_LWIP_IPV4
        if (from.ss_family == AF_INET) {
            // IPv4
            struct sockaddr_in *from4 = (struct sockaddr_in *)&from;
            inet_addr_to_ip4addr(ip_2_ip4(&recv_addr), &from4->sin_addr);
            IP_SET_TYPE_VAL(recv_addr, IPADDR_TYPE_V4);
            if (len < (int)sizeof(struct ip_hdr)) continue;
            const struct ip_hdr *iphdr = (const struct ip_hdr *)buf;
            unsigned header_len = IPH_HL_BYTES(iphdr);
            data_head = header_len + sizeof(struct icmp_echo_hdr);
            if (IPH_V(iphdr) != 4 || header_len < sizeof(struct ip_hdr) ||
                len < data_head || lwip_ntohs(IPH_LEN(iphdr)) < data_head ||
                IPH_PROTO(iphdr) != IP_PROTO_ICMP) continue;
            const struct sockaddr_in *target = (const struct sockaddr_in *)&ep->target_addr;
            if (ep->target_addr.ss_family != AF_INET || from4->sin_addr.s_addr != target->sin_addr.s_addr) continue;
            const struct icmp_echo_hdr *iecho = (const struct icmp_echo_hdr *)(buf + header_len);
            if (iecho->type == ICMP_ER && !iecho->code &&
                iecho->id == ep->packet_hdr->id && iecho->seqno == ep->packet_hdr->seqno) {
                ip_addr_copy(ep->recv_addr, recv_addr);
                ep->received++;
                ep->ttl = IPH_TTL(iphdr);
                ep->tos = IPH_TOS(iphdr);
                ep->recv_len = lwip_ntohs(IPH_LEN(iphdr)) - data_head;
                return len;
            }
        }
#endif
#if CONFIG_LWIP_IPV6
        if (from.ss_family == AF_INET6) {
            // IPv6
            struct sockaddr_in6 *from6 = (struct sockaddr_in6 *)&from;
            inet6_addr_to_ip6addr(ip_2_ip6(&recv_addr), &from6->sin6_addr);
            IP_SET_TYPE_VAL(recv_addr, IPADDR_TYPE_V6);
            data_head = (uint16_t)(sizeof(struct ip6_hdr) + sizeof(struct icmp6_echo_hdr));
            if (len >= data_head) {
                struct ip6_hdr *iphdr = (struct ip6_hdr *)buf;
                struct icmp6_echo_hdr *iecho6 = (struct icmp6_echo_hdr *)(buf + sizeof(struct ip6_hdr)); // IPv6 head length is 40
                if ((iecho6->type == ICMP6_TYPE_EREP) // only check the ICMPv6 echo reply types
                    && (iecho6->id == ep->packet_hdr->id) && (iecho6->seqno == ep->packet_hdr->seqno)) {
                    uint16_t payload_len = lwip_ntohs(IP6H_PLEN(iphdr));
                    if (payload_len < sizeof(struct icmp6_echo_hdr)) continue;
                    ip_addr_copy(ep->recv_addr, recv_addr);
                    ep->received++;
                    ep->recv_len = payload_len - sizeof(struct icmp6_echo_hdr);
                    return len;
                }
            }
        }
#endif // CONFIG_LWIP_IPV6
    }
    return -1;
}

static void esp_ping_thread(void *args)
{
    esp_ping_t *ep = (esp_ping_t *)(args);
    TickType_t last_wake;
    struct timeval start_time, end_time;
    int recv_ret;

    while (1) {
        /* wait for ping start signal */
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PING_CHECK_START_TIMEOUT_MS))) {
            /* initialize runtime statistics */
            ep->packet_hdr->seqno = 0;
            ep->transmitted = 0;
            ep->received = 0;
            ep->total_time_ms = 0;

            last_wake = xTaskGetTickCount();
            while ((ep->flags & PING_FLAGS_START) && ((ep->count == 0) || (ep->packet_hdr->seqno < ep->count))) {
                esp_ping_send(ep);
                gettimeofday(&start_time, NULL);
                recv_ret = esp_ping_receive(ep);
                gettimeofday(&end_time, NULL);
                ep->elapsed_time_ms = PING_TIME_DIFF_MS(end_time, start_time);
                ep->total_time_ms += ep->elapsed_time_ms;
                if (recv_ret >= 0) {
                    if (ep->on_ping_success) {
                        ep->on_ping_success((esp_ping_handle_t)ep, ep->cb_args);
                    }
                } else {
                    if (ep->on_ping_timeout) {
                        ep->on_ping_timeout((esp_ping_handle_t)ep, ep->cb_args);
                    }
                }
                if (pdMS_TO_TICKS(ep->interval_ms)) {
                    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ep->interval_ms)); // to get a more accurate delay
                }
            }
            /* batch of ping operations finished */
            if (ep->on_ping_end) {
                ep->on_ping_end((esp_ping_handle_t)ep, ep->cb_args);
            }
        } else {
            // check if ping has been de-initialized
            if (!(ep->flags & PING_FLAGS_INIT)) {
                break;
            }
        }
    }
    /* before exit task, free all resources */
    if (ep->packet_hdr) {
        free(ep->packet_hdr);
    }
    if (ep->sock > 0) {
        close(ep->sock);
    }
    free(ep);
    vTaskDelete(NULL);
}

esp_err_t esp_ping_new_session(const esp_ping_config_t *config, const esp_ping_callbacks_t *cbs, esp_ping_handle_t *hdl_out)
{
    esp_err_t ret = ESP_FAIL;
    esp_ping_t *ep = NULL;
    ESP_GOTO_ON_FALSE(config, ESP_ERR_INVALID_ARG, err, TAG, "ping config can't be null");
    ESP_GOTO_ON_FALSE(config->timeout_ms, ESP_ERR_INVALID_ARG, err, TAG, "ping timeout can't be zero");
    ESP_GOTO_ON_FALSE(hdl_out, ESP_ERR_INVALID_ARG, err, TAG, "ping handle can't be null");

    ep = mem_calloc(1, sizeof(esp_ping_t));
    ESP_GOTO_ON_FALSE(ep, ESP_ERR_NO_MEM, err, TAG, "no memory for esp_ping object");

    /* set INIT flag, so that ping task won't exit (must set before create ping task) */
    ep->flags |= PING_FLAGS_INIT;

    /* create ping thread */
    BaseType_t xReturned = xTaskCreate(esp_ping_thread, "ping", config->task_stack_size, ep,
                                       config->task_prio, &ep->ping_task_hdl);
    ESP_GOTO_ON_FALSE(xReturned == pdTRUE, ESP_ERR_NO_MEM, err, TAG, "create ping task failed");

    /* callback functions */
    if (cbs) {
        ep->cb_args = cbs->cb_args;
        ep->on_ping_end = cbs->on_ping_end;
        ep->on_ping_timeout = cbs->on_ping_timeout;
        ep->on_ping_success = cbs->on_ping_success;
    }
    /* set parameters for ping */
    ep->recv_addr = config->target_addr;
    ep->count = config->count;
    ep->interval_ms = config->interval_ms;
    ep->timeout_ms = config->timeout_ms;
    ep->icmp_pkt_size = sizeof(struct icmp_echo_hdr) + config->data_size;
    ep->packet_hdr = mem_calloc(1, ep->icmp_pkt_size);
    ESP_GOTO_ON_FALSE(ep->packet_hdr,ESP_ERR_NO_MEM, err, TAG, "no memory for echo packet");
    /* set ICMP type and code field */
    ep->packet_hdr->code = 0;
    /* ping id should be unique, treat task handle as ping ID */
    ep->packet_hdr->id = ((intptr_t)ep->ping_task_hdl) & 0xFFFF;
    /* fill the additional data buffer with some data */
    char *d = (char *)(ep->packet_hdr) + sizeof(struct icmp_echo_hdr);
    for (uint32_t i = 0; i < config->data_size; i++) {
        d[i] = 'A' + i;
    }

    /* create socket */
    if (IP_IS_V4(&config->target_addr)
#if CONFIG_LWIP_IPV6
        || ip6_addr_isipv4mappedipv6(ip_2_ip6(&config->target_addr))
#endif
    ) {
        ep->sock = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    }
#if CONFIG_LWIP_IPV6
    else {
        ep->sock = socket(AF_INET6, SOCK_RAW, IP6_NEXTH_ICMP6);
    }
#endif
    ESP_GOTO_ON_FALSE(ep->sock >= 0, ESP_FAIL, err, TAG, "create socket failed: %d", ep->sock);
    /* set if index */
    if(config->interface) {
        struct ifreq iface;
        if (if_indextoname(config->interface, iface.ifr_name) == NULL) {
            ESP_LOGE(TAG, "fail to find interface name with netif index %" PRIu32, config->interface);
            goto err;
        }
        if(setsockopt(ep->sock, SOL_SOCKET, SO_BINDTODEVICE, &iface, sizeof(iface)) != 0) {
            ESP_LOGE(TAG, "fail to setsockopt SO_BINDTODEVICE");
            goto err;
        }
    }
    struct timeval timeout;
    timeout.tv_sec = config->timeout_ms / 1000;
    timeout.tv_usec = (config->timeout_ms % 1000) * 1000;
    /* set receive timeout */
    setsockopt(ep->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    /* set tos */
    setsockopt(ep->sock, IPPROTO_IP, IP_TOS, &config->tos, sizeof(config->tos));

    /* set ttl */
    setsockopt(ep->sock, IPPROTO_IP, IP_TTL, &config->ttl, sizeof(config->ttl));

    /* set socket address */
#if CONFIG_LWIP_IPV4
    if (IP_IS_V4(&config->target_addr)) {
        struct sockaddr_in *to4 = (struct sockaddr_in *)&ep->target_addr;
        to4->sin_family = AF_INET;
        inet_addr_from_ip4addr(&to4->sin_addr, ip_2_ip4(&config->target_addr));
        ep->packet_hdr->type = ICMP_ECHO;
    }
#endif
#if CONFIG_LWIP_IPV6
    if (IP_IS_V6(&config->target_addr)) {
        struct sockaddr_in6 *to6 = (struct sockaddr_in6 *)&ep->target_addr;
        to6->sin6_family = AF_INET6;
        inet6_addr_from_ip6addr(&to6->sin6_addr, ip_2_ip6(&config->target_addr));
        ep->packet_hdr->type = ICMP6_TYPE_EREQ;
    }
#endif
    /* return ping handle to user */
    *hdl_out = (esp_ping_handle_t)ep;
    return ESP_OK;
err:
    if (ep) {
        if (ep->sock > 0) {
            close(ep->sock);
        }
        if (ep->packet_hdr) {
            free(ep->packet_hdr);
        }
        if (ep->ping_task_hdl) {
            vTaskDelete(ep->ping_task_hdl);
        }
        free(ep);
    }
    return ret;
}

esp_err_t esp_ping_delete_session(esp_ping_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_ping_t *ep = (esp_ping_t *)hdl;
    ESP_GOTO_ON_FALSE(ep, ESP_ERR_INVALID_ARG, err, TAG, "ping handle can't be null");
    /* reset init flags, then ping task will exit */
    ep->flags &= ~PING_FLAGS_INIT;
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_ping_start(esp_ping_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_ping_t *ep = (esp_ping_t *)hdl;
    ESP_GOTO_ON_FALSE(ep, ESP_ERR_INVALID_ARG, err, TAG, "ping handle can't be null");
    ep->flags |= PING_FLAGS_START;
    xTaskNotifyGive(ep->ping_task_hdl);
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_ping_stop(esp_ping_handle_t hdl)
{
    esp_err_t ret = ESP_OK;
    esp_ping_t *ep = (esp_ping_t *)hdl;
    ESP_GOTO_ON_FALSE(ep, ESP_ERR_INVALID_ARG, err, TAG, "ping handle can't be null");
    ep->flags &= ~PING_FLAGS_START;
    return ESP_OK;
err:
    return ret;
}

esp_err_t esp_ping_get_profile(esp_ping_handle_t hdl, esp_ping_profile_t profile, void *data, uint32_t size)
{
    esp_err_t ret = ESP_OK;
    esp_ping_t *ep = (esp_ping_t *)hdl;
    const void *from = NULL;
    uint32_t copy_size = 0;
    ESP_GOTO_ON_FALSE(ep, ESP_ERR_INVALID_ARG, err, TAG, "ping handle can't be null");
    ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "profile data can't be null");
    switch (profile) {
    case ESP_PING_PROF_SEQNO:
        from = &ep->packet_hdr->seqno;
        copy_size = sizeof(ep->packet_hdr->seqno);
        break;
    case ESP_PING_PROF_TOS:
        from = &ep->tos;
        copy_size = sizeof(ep->tos);
        break;
    case ESP_PING_PROF_TTL:
        from = &ep->ttl;
        copy_size = sizeof(ep->ttl);
        break;
    case ESP_PING_PROF_REQUEST:
        from = &ep->transmitted;
        copy_size = sizeof(ep->transmitted);
        break;
    case ESP_PING_PROF_REPLY:
        from = &ep->received;
        copy_size = sizeof(ep->received);
        break;
    case ESP_PING_PROF_IPADDR:
        from = &ep->recv_addr;
        copy_size = sizeof(ep->recv_addr);
        break;
    case ESP_PING_PROF_SIZE:
        from = &ep->recv_len;
        copy_size = sizeof(ep->recv_len);
        break;
    case ESP_PING_PROF_TIMEGAP:
        from = &ep->elapsed_time_ms;
        copy_size = sizeof(ep->elapsed_time_ms);
        break;
    case ESP_PING_PROF_DURATION:
        from = &ep->total_time_ms;
        copy_size = sizeof(ep->total_time_ms);
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG, "unknown profile: %d", profile);
        break;
    }
    ESP_GOTO_ON_FALSE(size >= copy_size, ESP_ERR_INVALID_SIZE, err, TAG, "unmatched data size for profile %d", profile);
    memcpy(data, from, copy_size);
    return ESP_OK;
err:
    return ret;
}
