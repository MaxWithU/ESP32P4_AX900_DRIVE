// SPDX-License-Identifier: Apache-2.0
#include "ax900_profile.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/platform_util.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#include "esp_flash_encrypt.h"
#include "esp_partition.h"
#endif
#ifndef CONFIG_AX900_PROFILE_PARTITION
#define CONFIG_AX900_PROFILE_PARTITION "nvs"
#endif

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
static bool encrypted;
static esp_err_t open_profiles(int mode,nvs_handle_t *nvs) {
#ifdef ESP_PLATFORM
    return nvs_open_from_partition(CONFIG_AX900_PROFILE_PARTITION,"ax900_wifi",mode,nvs);
#else
    return nvs_open("ax900_wifi",mode,nvs);
#endif
}
static const char *auto_keys[PROFILE_SLOTS]={"auto0","auto1","auto2","auto3"};
static bool auto_enabled(nvs_handle_t nvs,unsigned slot){uint8_t v=1;return nvs_get_u8(nvs,auto_keys[slot],&v)==ESP_OK?v==1:true;}
bool ax900_profiles_encrypted(void){return encrypted;}

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
    esp_err_t e;
#if defined(ESP_PLATFORM) && CONFIG_AX900_ENCRYPTED_PROFILES
#if !CONFIG_NVS_ENCRYPTION
#error "Encrypted AX900 profiles require CONFIG_NVS_ENCRYPTION"
#endif
    // Only consume already provisioned, flash-encrypted keys. Never generate keys,
    // burn eFuses, erase NVS, or silently fall back to plaintext.
    if(!esp_flash_encryption_enabled())return ESP_ERR_NOT_SUPPORTED;
    // IDF secure_init returns success for an already initialized plaintext store.
    // Own a dedicated, unopened partition so that success really means encrypted.
    if(!strcmp(CONFIG_AX900_PROFILE_PARTITION,"nvs"))return ESP_ERR_INVALID_ARG;
    nvs_stats_t stats;
    e=nvs_get_stats(CONFIG_AX900_PROFILE_PARTITION,&stats);
    if(e!=ESP_ERR_NVS_NOT_INITIALIZED)return e==ESP_OK?ESP_ERR_INVALID_STATE:e;
    const esp_partition_t *partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS,CONFIG_AX900_PROFILE_KEYS_PARTITION);
    if(!partition || !partition->encrypted)return ESP_ERR_NOT_FOUND;
    nvs_sec_cfg_t keys={0};e=nvs_flash_read_security_cfg(partition,&keys);
    if(e==ESP_OK)e=nvs_flash_secure_init_partition(CONFIG_AX900_PROFILE_PARTITION,&keys);
    mbedtls_platform_zeroize(&keys,sizeof(keys));
    if(e==ESP_OK)encrypted=true;
#elif defined(ESP_PLATFORM)
    // Preserve the application's default NVS encryption configuration.
    e=!strcmp(CONFIG_AX900_PROFILE_PARTITION,"nvs")?nvs_flash_init():nvs_flash_init_partition(CONFIG_AX900_PROFILE_PARTITION);
#else
    e=nvs_flash_init();
#endif
    if(e!=ESP_OK)return e;
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
    nvs_handle_t nvs;esp_err_t e=open_profiles(NVS_READWRITE,&nvs);
    if(e==ESP_OK) {
        uint8_t last=PROFILE_SLOTS-1;nvs_get_u8(nvs,"last",&last);if(last>=PROFILE_SLOTS)last=PROFILE_SLOTS-1;
        unsigned slot=(last+1)%PROFILE_SLOTS;bool equal=false,existing=false;int empty=-1;
        for(unsigned i=0;i<PROFILE_SLOTS;i++) {
            size_t old_size;profile_t *old=read_profile(nvs,i,&old_size);
            if(!old){if(empty<0)empty=i;continue;}
            bool match=matches(old,&r->ap);
            if(match){existing=true;slot=i;equal=old_size==size && !memcmp(old,p,size);}
            wipe(old,old_size);if(match){empty=-1;break;}
        }
        if(empty>=0)slot=(unsigned)empty;
        if(!equal)e=nvs_set_blob(nvs,keys[slot],p,size);
        if(e==ESP_OK && !existing)e=nvs_set_u8(nvs,auto_keys[slot],1);
        if(e==ESP_OK && last!=slot)e=nvs_set_u8(nvs,"last",slot);
        if(e==ESP_OK)e=nvs_commit(nvs);
        nvs_close(nvs);
    }
    xSemaphoreGive(mutex);wipe(p,size);return e;
}
static ax_connect_request_t *find_profile(const ax900_ap_t *aps,size_t count,bool automatic) {
    if(!mutex || !aps || !count)return NULL;
    ax_connect_request_t *r=NULL;
    xSemaphoreTake(mutex,portMAX_DELAY);
    nvs_handle_t nvs;
    if(open_profiles(NVS_READONLY,&nvs)==ESP_OK) {
        uint8_t last=0;nvs_get_u8(nvs,"last",&last);if(last>=PROFILE_SLOTS)last=0;
        for(unsigned k=0;k<PROFILE_SLOTS && !r;k++) {
            unsigned slot=(last+PROFILE_SLOTS-k)%PROFILE_SLOTS;
            if(automatic && !auto_enabled(nvs,slot))continue;
            size_t size;profile_t *p=read_profile(nvs,slot,&size);
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
static unsigned count_profiles(bool automatic) {
    if(!mutex)return 0;
    unsigned count=0;xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;
    if(open_profiles(NVS_READONLY,&nvs)==ESP_OK) {
        for(unsigned i=0;i<PROFILE_SLOTS;i++){size_t n;profile_t *p=read_profile(nvs,i,&n);if(p){if(!automatic || auto_enabled(nvs,i))count++;wipe(p,n);}}
        nvs_close(nvs);
    }
    xSemaphoreGive(mutex);return count;
}
unsigned ax_profile_count(void){return count_profiles(false);}
unsigned ax_profile_auto_count(void){return count_profiles(true);}
esp_err_t ax_profile_forget(void) {
    if(!mutex)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;esp_err_t e=open_profiles(NVS_READWRITE,&nvs);
    if(e==ESP_OK){e=nvs_erase_all(nvs);if(e==ESP_OK)e=nvs_commit(nvs);nvs_close(nvs);}
    xSemaphoreGive(mutex);return e;
}

ax_connect_request_t *ax_profile_find(const ax900_ap_t *aps,size_t count){return find_profile(aps,count,false);}
ax_connect_request_t *ax_profile_find_auto(const ax900_ap_t *aps,size_t count){return find_profile(aps,count,true);}
size_t ax900_list_saved(ax900_saved_network_t *out,size_t capacity){
    if(!mutex)return 0;size_t count=0;xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;
    if(open_profiles(NVS_READONLY,&nvs)==ESP_OK){
        for(unsigned i=0;i<PROFILE_SLOTS;i++){
            size_t n;profile_t *p=read_profile(nvs,i,&n);if(!p)continue;
            if(out && count<capacity){
                ax900_saved_network_t *o=&out[count];memset(o,0,sizeof(*o));
                o->network.ssid_len=p->ssid_len;memcpy(o->network.raw_ssid,p->ssid,p->ssid_len);
                for(unsigned j=0;j<p->ssid_len;j++)o->network.ssid[j]=p->ssid[j]<32 || p->ssid[j]==127?'?':p->ssid[j];
                o->network.secured=p->secured;o->network.enterprise=p->enterprise;o->network.wpa2_psk=p->secured&&!p->enterprise;
                o->auto_connect=auto_enabled(nvs,i);o->verifies_server=p->enterprise && p->ca[0] && !p->unverified;
            }
            count++;wipe(p,n);
        }nvs_close(nvs);
    }xSemaphoreGive(mutex);return count;
}
static esp_err_t update_profile(const ax900_ap_t *ap,bool erase,bool enabled){
    if(!mutex)return ESP_ERR_INVALID_STATE;
    if(!ap || !ap->ssid_len || ap->ssid_len>32)return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;esp_err_t e=open_profiles(NVS_READWRITE,&nvs);
    if(e==ESP_OK){
        e=ESP_ERR_NOT_FOUND;
        for(unsigned i=0;i<PROFILE_SLOTS;i++){
            size_t n;profile_t *p=read_profile(nvs,i,&n);if(!p)continue;
            bool match=matches(p,ap);wipe(p,n);if(!match)continue;
            e=erase?nvs_erase_key(nvs,keys[i]):nvs_set_u8(nvs,auto_keys[i],enabled);
            if(erase && e==ESP_OK){esp_err_t removed=nvs_erase_key(nvs,auto_keys[i]);if(removed!=ESP_OK && removed!=ESP_ERR_NVS_NOT_FOUND)e=removed;}
            if(e==ESP_OK)e=nvs_commit(nvs);break;
        }nvs_close(nvs);
    }xSemaphoreGive(mutex);return e;
}
esp_err_t ax_profile_forget_network(const ax900_ap_t *ap){return update_profile(ap,true,false);}
esp_err_t ax_profile_set_auto(const ax900_ap_t *ap,bool enabled){return update_profile(ap,false,enabled);}
bool ax_profile_auto_enabled(const ax900_ap_t *ap){
    if(!mutex)return true;bool enabled=true;xSemaphoreTake(mutex,portMAX_DELAY);nvs_handle_t nvs;
    if(open_profiles(NVS_READONLY,&nvs)==ESP_OK){
        for(unsigned i=0;i<PROFILE_SLOTS;i++){
            size_t n;profile_t *p=read_profile(nvs,i,&n);if(!p)continue;
            bool match=matches(p,ap);wipe(p,n);if(match){enabled=auto_enabled(nvs,i);break;}
        }nvs_close(nvs);
    }xSemaphoreGive(mutex);return enabled;
}
