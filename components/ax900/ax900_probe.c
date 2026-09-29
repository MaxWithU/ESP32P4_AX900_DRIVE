// SPDX-License-Identifier: Apache-2.0
#include "ax900.h"
#include "ax900_probe.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "ax900_ping.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static portMUX_TYPE probe_lock = portMUX_INITIALIZER_UNLOCKED;
static ax900_probe_result_t result;
struct probe_context {
    uint32_t connection_id, sum_ms;
    int64_t started;
    bool stale;
};

void ax900_probe_get_result(ax900_probe_result_t *out) {
    if(!out)return;
    taskENTER_CRITICAL(&probe_lock);*out=result;taskEXIT_CRITICAL(&probe_lock);
    if(out->connection_id && out->state!=AX900_PROBE_RUNNING && !ax900_connection_is_current(out->connection_id))
        out->state=AX900_PROBE_STALE;
}

static void record_reply(esp_ping_handle_t handle,void *arg,bool received) {
    struct probe_context *ctx=arg;
    uint32_t sent=0,replies=0,rtt=0;
    esp_ping_get_profile(handle,ESP_PING_PROF_REQUEST,&sent,sizeof(sent));
    esp_ping_get_profile(handle,ESP_PING_PROF_REPLY,&replies,sizeof(replies));
    if(received)esp_ping_get_profile(handle,ESP_PING_PROF_TIMEGAP,&rtt,sizeof(rtt));
    if(!ax900_connection_is_current(ctx->connection_id)) {ctx->stale=true;esp_ping_stop(handle);}
    taskENTER_CRITICAL(&probe_lock);
    result.sent=sent;result.received=replies;
    if(sent && sent<=AX900_PROBE_COUNT)result.rtt_ms[sent-1]=received?(int32_t)rtt:-2;
    if(received) {
        ctx->sum_ms+=rtt;
        if(replies==1 || rtt<result.min_ms)result.min_ms=rtt;
        if(rtt>result.max_ms)result.max_ms=rtt;
        result.average_ms=ctx->sum_ms/replies;
    }
    result.elapsed_ms=(esp_timer_get_time()-ctx->started)/1000;
    taskEXIT_CRITICAL(&probe_lock);
}
static void received(esp_ping_handle_t handle,void *arg){record_reply(handle,arg,true);}
static void timeout(esp_ping_handle_t handle,void *arg){record_reply(handle,arg,false);}
static void finished(esp_ping_handle_t handle,void *arg) {
    struct probe_context *ctx=arg;
    bool stale=ctx->stale || !ax900_connection_is_current(ctx->connection_id);
    esp_ping_delete_session(handle);
    taskENTER_CRITICAL(&probe_lock);
    result.elapsed_ms=(esp_timer_get_time()-ctx->started)/1000;
    result.state=stale?AX900_PROBE_STALE:result.received==AX900_PROBE_COUNT?AX900_PROBE_PASSED:
                 result.received?AX900_PROBE_PARTIAL:AX900_PROBE_FAILED;
    uint32_t sent=result.sent,replies=result.received,average=result.average_ms;
    taskEXIT_CRITICAL(&probe_lock);
    ESP_LOGI("AX900","PROBE_DONE sent=%lu received=%lu average_ms=%lu stale=%u",
             (unsigned long)sent,(unsigned long)replies,(unsigned long)average,stale);
    free(ctx);
}

static esp_err_t failed_start(esp_err_t error) {
    ESP_LOGW("AX900","Probe start failed: %d",error);
    taskENTER_CRITICAL(&probe_lock);result.state=AX900_PROBE_FAILED;result.error=error;taskEXIT_CRITICAL(&probe_lock);
    return error;
}
esp_err_t ax900_probe_start(void) {
    taskENTER_CRITICAL(&probe_lock);
    if(result.state==AX900_PROBE_RUNNING){taskEXIT_CRITICAL(&probe_lock);return ESP_ERR_INVALID_STATE;}
    memset(&result,0,sizeof(result));result.state=AX900_PROBE_RUNNING;
    for(unsigned i=0;i<AX900_PROBE_COUNT;i++)result.rtt_ms[i]=-1;
    taskEXIT_CRITICAL(&probe_lock);

    ax900_link_status_t link;ax900_get_link_status(&link);
    uint32_t connection_id=link.connection_id;
    bool ready=link.associated && link.authenticated && link.has_ip;
    if(!ready)return failed_start(ESP_ERR_INVALID_STATE);
    esp_netif_t *netif=esp_netif_get_handle_from_ifkey("AX900");
    esp_netif_ip_info_t ip={0};
    if(!ready || !netif || esp_netif_get_ip_info(netif,&ip)!=ESP_OK || !ip.ip.addr || !ip.gw.addr)
        return failed_start(ESP_ERR_INVALID_STATE);
    int index=esp_netif_get_netif_impl_index(netif);
    if(index<=0 || !ax900_connection_is_current(connection_id))return failed_start(ESP_ERR_INVALID_STATE);
    struct probe_context *ctx=calloc(1,sizeof(*ctx));
    if(!ctx)return failed_start(ESP_ERR_NO_MEM);
    ctx->connection_id=connection_id;ctx->started=esp_timer_get_time();
    taskENTER_CRITICAL(&probe_lock);
    result.connection_id=connection_id;
    snprintf(result.gateway,sizeof(result.gateway),IPSTR,IP2STR(&ip.gw));
    taskEXIT_CRITICAL(&probe_lock);
    esp_ping_config_t config=ESP_PING_DEFAULT_CONFIG();
    config.count=AX900_PROBE_COUNT;config.interval_ms=500;config.timeout_ms=1500;
    config.target_addr.type=IPADDR_TYPE_V4;config.target_addr.u_addr.ip4.addr=ip.gw.addr;config.interface=index;
    esp_ping_callbacks_t callbacks={.on_ping_success=received,.on_ping_timeout=timeout,.on_ping_end=finished,.cb_args=ctx};
    esp_ping_handle_t handle=NULL;
    esp_err_t error=esp_ping_new_session(&config,&callbacks,&handle);
    if(error==ESP_OK) {
        error=esp_ping_start(handle);
        if(error!=ESP_OK)esp_ping_delete_session(handle);
    }
    if(error!=ESP_OK){free(ctx);return failed_start(error);}
    return ESP_OK;
}
