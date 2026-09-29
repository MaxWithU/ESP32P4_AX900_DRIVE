// SPDX-License-Identifier: Apache-2.0
#include "ax900_pool.h"
#include "ax900_metrics.h"
#include "freertos/FreeRTOS.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
static ax_packet_t *packets;
static uint32_t occupied[2];
static unsigned in_use;
static portMUX_TYPE pool_lock=portMUX_INITIALIZER_UNLOCKED;
esp_err_t ax_pool_init(void){
    if(packets)return ESP_OK;
#ifdef ESP_PLATFORM
    packets=heap_caps_calloc(AX_PACKET_COUNT,sizeof(*packets),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!packets)packets=heap_caps_calloc(AX_PACKET_COUNT,sizeof(*packets),MALLOC_CAP_8BIT);
#else
    packets=calloc(AX_PACKET_COUNT,sizeof(*packets));
#endif
    return packets?ESP_OK:ESP_ERR_NO_MEM;
}
ax_packet_t *ax_pool_acquire(ax_packet_kind_t kind){
    // Independent reservations prevent application traffic starving authentication.
    unsigned begin=kind==AX_PACKET_TX?0:kind==AX_PACKET_RX?24:56;
    unsigned end=kind==AX_PACKET_TX?24:kind==AX_PACKET_RX?56:64;
    ax_packet_t *p=NULL;taskENTER_CRITICAL(&pool_lock);
    if(packets)for(unsigned i=begin;i<end;i++)if(!(occupied[i/32]&(UINT32_C(1)<<(i%32)))){
        occupied[i/32]|=UINT32_C(1)<<(i%32);p=&packets[i];in_use++;
        ax_metric_add(AX900_POOL_USED,1);ax_metric_peak(AX900_POOL_PEAK,in_use);break;
    }
    taskEXIT_CRITICAL(&pool_lock);return p;
}
void ax_pool_release(ax_packet_t *p){
    if(!p)return;
    taskENTER_CRITICAL(&pool_lock);
    assert(packets && p>=packets && p<packets+AX_PACKET_COUNT);
    unsigned i=p-packets;uint32_t mask=UINT32_C(1)<<(i%32);
    assert(occupied[i/32]&mask);
    if(i>=56)memset(p->bytes,0,sizeof(p->bytes));
    occupied[i/32]&=~mask;in_use--;ax_metric_add(AX900_POOL_USED,-1);
    taskEXIT_CRITICAL(&pool_lock);
}
unsigned ax_pool_used(void){taskENTER_CRITICAL(&pool_lock);unsigned n=in_use;taskEXIT_CRITICAL(&pool_lock);return n;}
esp_err_t ax_pool_destroy(void){
    taskENTER_CRITICAL(&pool_lock);
    if(in_use){taskEXIT_CRITICAL(&pool_lock);return ESP_ERR_INVALID_STATE;}
    ax_packet_t *old=packets;packets=NULL;taskEXIT_CRITICAL(&pool_lock);
    free(old);return ESP_OK;
}
