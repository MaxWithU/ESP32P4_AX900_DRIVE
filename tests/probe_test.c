// SPDX-License-Identifier: Apache-2.0
// Drive the production module through real callbacks with a fake ping transport.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ax900.h"
#include "ax900_probe.h"
#include "esp_netif.h"
#include "ping/ping_sock.h"

static bool online;
static uint32_t epoch=1,sent,replies,rtt;
static int created,deleted,stopped,fail_create,fail_start;
static int64_t now;
static esp_netif_t iface;
static esp_ping_callbacks_t callbacks;
int64_t esp_timer_get_time(void){return now;}
bool ax900_connection_is_current(uint32_t id){return online && id==epoch;}
void ax900_get_link_status(ax900_link_status_t *s){memset(s,0,sizeof(*s));s->connection_id=epoch;s->associated=s->authenticated=s->has_ip=online;}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key){assert(!strcmp(key,"AX900"));return &iface;}
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *ip){assert(n==&iface);ip->ip.addr=2;ip->gw.addr=1;return ESP_OK;}
int esp_netif_get_netif_impl_index(esp_netif_t *n){assert(n==&iface);return 7;}
esp_err_t esp_ping_new_session(const esp_ping_config_t *c,const esp_ping_callbacks_t *cb,esp_ping_handle_t *h){
    assert(c->interface==7 && c->target_addr.u_addr.ip4.addr==1 && c->count==5);
    assert(c->timeout_ms>0 && c->timeout_ms<=1500);
    if(fail_create)return ESP_FAIL;
    callbacks=*cb;*h=&iface;created++;sent=replies=rtt=0;return ESP_OK;
}
esp_err_t esp_ping_start(esp_ping_handle_t h){assert(h==&iface);return fail_start?ESP_FAIL:ESP_OK;}
esp_err_t esp_ping_stop(esp_ping_handle_t h){assert(h==&iface);stopped++;return ESP_OK;}
esp_err_t esp_ping_delete_session(esp_ping_handle_t h){assert(h==&iface);deleted++;return ESP_OK;}
esp_err_t esp_ping_get_profile(esp_ping_handle_t h,esp_ping_profile_t field,void *p,size_t n){
    assert(h==&iface && n==sizeof(uint32_t));
    *(uint32_t *)p=field==ESP_PING_PROF_REQUEST?sent:field==ESP_PING_PROF_REPLY?replies:rtt;return ESP_OK;
}
static void reply(int delay){
    sent++;now+=500000;
    if(delay<0)callbacks.on_ping_timeout(&iface,callbacks.cb_args);
    else {replies++;rtt=delay;callbacks.on_ping_success(&iface,callbacks.cb_args);}
}
static ax900_probe_result_t snapshot(void){ax900_probe_result_t p;ax900_probe_get_result(&p);return p;}
static void finish(void){callbacks.on_ping_end(&iface,callbacks.cb_args);}
int main(void){
    assert(snapshot().state==AX900_PROBE_IDLE);
    assert(ax900_probe_start()==ESP_ERR_INVALID_STATE && created==0);
    online=true;
    assert(ax900_probe_start()==ESP_OK);
    assert(ax900_probe_start()==ESP_ERR_INVALID_STATE && created==1);
    reply(12);reply(-1);reply(0);reply(38);reply(10);finish();
    ax900_probe_result_t p=snapshot();
    assert(p.state==AX900_PROBE_PARTIAL && p.sent==5 && p.received==4);
    assert(p.average_ms==15 && p.min_ms==0 && p.max_ms==38 && p.rtt_ms[1]==-2 && p.rtt_ms[2]==0);
    assert(deleted==1);
    assert(ax900_probe_start()==ESP_OK);
    for(unsigned i=0;i<5;i++)reply(5);
    finish();assert(snapshot().state==AX900_PROBE_PASSED);
    epoch++;assert(snapshot().state==AX900_PROBE_STALE);
    assert(ax900_probe_start()==ESP_OK);
    reply(1);epoch++;reply(1);finish();
    assert(stopped==1 && snapshot().state==AX900_PROBE_STALE);
    assert(ax900_probe_start()==ESP_OK);
    for(unsigned i=0;i<5;i++)reply(-1);
    finish();assert(snapshot().state==AX900_PROBE_FAILED && snapshot().received==0);
    fail_create=1;assert(ax900_probe_start()==ESP_FAIL);fail_create=0;
    fail_start=1;assert(ax900_probe_start()==ESP_FAIL);fail_start=0;
    assert(created==deleted);
    assert(ax900_probe_start()==ESP_OK);reply(1);finish();
    assert(created==deleted);
    puts("Probe API: interface binding, busy gate, RTT/loss, disconnect freshness and cleanup passed");
}
