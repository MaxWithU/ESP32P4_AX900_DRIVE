// SPDX-License-Identifier: Apache-2.0
#include "ax900_profile.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/platform_util.h"
#include <stdlib.h>
#include <string.h>

#define PROFILE_SLOTS 4
// Small profiles use about 600 bytes. The optional CA occupies only its actual length.
typedef struct {
    uint32_t version;
    uint8_t ssid_len, secured, enterprise, unverified;
    uint8_t ssid[32];
    char username[129], password[129], server_name[254];
    char ca[];
} profile_t;
static SemaphoreHandle_t mutex;
static const char *keys[PROFILE_SLOTS]={"wifi0","wifi1","wifi2","wifi3"};
static void wipe(void *p,size_t n){if(p){mbedtls_platform_zeroize(p,n);free(p);}}
static profile_t *read_profile(nvs_handle_t nvs,unsigned slot,size_t *size) {
    *size=0;
    if(nvs_get_blob(nvs,keys[slot],NULL,size)!=ESP_OK || *size<sizeof(profile_t)+1 || *size>sizeof(profile_t)+8193)return NULL;
    profile_t *p=calloc(1,*size);
    if(!p)return NULL;
    if(nvs_get_blob(nvs,keys[slot],p,size)!=ESP_OK || p->version!=1 || !p->ssid_len || p->ssid_len>32 ||
       p->secured>1 || p->enterprise>1 || p->unverified>1 || (p->enterprise && !p->secured) ||
       !memchr(p->username,0,sizeof(p->username)) || !memchr(p->password,0,sizeof(p->password)) ||
       !memchr(p->server_name,0,sizeof(p->server_name)) || !memchr(p->ca,0,*size-sizeof(*p))) {
        wipe(p,*size);return NULL;
    }
    return p;
}
static bool matches(const profile_t *p,const ax900_ap_t *ap) {
    return p->ssid_len==ap->ssid_len && !memcmp(p->ssid,ap->raw_ssid,p->ssid_len) &&
           p->secured==ap->secured && p->enterprise==ap->enterprise && !ap->pmf_required &&
           (!ap->secured || ap->enterprise || ap->wpa2_psk);
}
esp_err_t ax_profile_init(void) {
    if(mutex)return ESP_OK;
    // Never erase another application's NVS to recover an initialization error.
    esp_err_t e=nvs_flash_init();if(e!=ESP_OK)return e;
    mutex=xSemaphoreCreateMutex();return mutex?ESP_OK:ESP_ERR_NO_MEM;
}
esp_err_t ax_profile_save(const ax_connect_request_t *r) {
    if(!mutex)return ESP_ERR_INVALID_STATE;
    if(!r || r->association_test || r->skip_save)return ESP_ERR_INVALID_ARG;
    size_t size=sizeof(profile_t)+strlen(r->ca_pem)+1;
    profile_t *p=calloc(1,size);if(!p)return ESP_ERR_NO_MEM;
    p->version=1;p->ssid_len=r->ap.ssid_len;p->secured=r->ap.secured;p->enterprise=r->enterprise;p->unverified=r->allow_unverified_server;
    memcpy(p->ssid,r->ap.raw_ssid,p->ssid_len);memcpy(p->username,r->username,sizeof(p->username));
    memcpy(p->password,r->password,sizeof(p->password));memcpy(p->server_name,r->server_name,sizeof(p->server_name));
    memcpy(p->ca,r->ca_pem,size-sizeof(*p));
    xSemaphoreTake(mutex,portMAX_DELAY);
    nvs_handle_t nvs;esp_err_t e=nvs_open("ax900_wifi",NVS_READWRITE,&nvs);
    if(e==ESP_OK) {
        uint8_t last=PROFILE_SLOTS-1;nvs_get_u8(nvs,"last",&last);if(last>=PROFILE_SLOTS)last=PROFILE_SLOTS-1;
        unsigned slot=(last+1)%PROFILE_SLOTS;bool equal=false;int empty=-1;
        for(unsigned i=0;i<PROFILE_SLOTS;i++) {
            size_t old_size;profile_t *old=read_profile(nvs,i,&old_size);
            if(!old){if(empty<0)empty=i;continue;}
            bool match=matches(old,&r->ap);
            if(match){slot=i;equal=old_size==size && !memcmp(old,p,size);}
            wipe(old,old_size);if(match){empty=-1;break;}
        }
        if(empty>=0)slot=(unsigned)empty;
        if(!equal)e=nvs_set_blob(nvs,keys[slot],p,size);
        if(e==ESP_OK && last!=slot)e=nvs_set_u8(nvs,"last",slot);
        if(e==ESP_OK)e=nvs_commit(nvs);
        nvs_close(nvs);
    }
    xSemaphoreGive(mutex);wipe(p,size);return e;
}
ax_connect_request_t *ax_profile_find(const ax900_ap_t *aps,size_t count) {
    if(!mutex || !aps || !count)return NULL;
    ax_connect_request_t *r=NULL;
    xSemaphoreTake(mutex,portMAX_DELAY);
    nvs_handle_t nvs;
    if(nvs_open("ax900_wifi",NVS_READONLY,&nvs)==ESP_OK) {
        uint8_t last=0;nvs_get_u8(nvs,"last",&last);if(last>=PROFILE_SLOTS)last=0;
        for(unsigned k=0;k<PROFILE_SLOTS && !r;k++) {
            size_t size;profile_t *p=read_profile(nvs,(last+PROFILE_SLOTS-k)%PROFILE_SLOTS,&size);
            if(!p)continue;
            const ax900_ap_t *best=NULL;
            for(size_t i=0;i<count;i++)if(matches(p,&aps[i]) && (!best ||
                (aps[i].frequency>5000 && best->frequency<=5000) ||
                ((aps[i].frequency>5000)==(best->frequency>5000) && aps[i].rssi>best->rssi)))best=&aps[i];
            if(best) {
                r=calloc(1,sizeof(*r));
                if(r){r->ap=*best;r->enterprise=p->enterprise;r->allow_unverified_server=p->unverified;
                    memcpy(r->username,p->username,sizeof(r->username));memcpy(r->password,p->password,sizeof(r->password));
                    memcpy(r->server_name,p->server_name,sizeof(r->server_name));memcpy(r->ca_pem,p->ca,size-sizeof(*p));}
            }
            wipe(p,size);
        }
        nvs_close(nvs);
    }
    xSemaphoreGive(mutex);return r;
}
unsigned ax_profile_count(void) {
    if(!mutex)return 0;
    unsigned count=0;xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;
    if(nvs_open("ax900_wifi",NVS_READONLY,&nvs)==ESP_OK) {
        for(unsigned i=0;i<PROFILE_SLOTS;i++){size_t n;profile_t *p=read_profile(nvs,i,&n);if(p){count++;wipe(p,n);}}
        nvs_close(nvs);
    }
    xSemaphoreGive(mutex);return count;
}
esp_err_t ax_profile_forget(void) {
    if(!mutex)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;esp_err_t e=nvs_open("ax900_wifi",NVS_READWRITE,&nvs);
    if(e==ESP_OK){e=nvs_erase_all(nvs);if(e==ESP_OK)e=nvs_commit(nvs);nvs_close(nvs);}
    xSemaphoreGive(mutex);return e;
}
