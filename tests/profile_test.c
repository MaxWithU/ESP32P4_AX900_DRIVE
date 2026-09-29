// SPDX-License-Identifier: Apache-2.0
// Only synthetic credentials are used; never print their content.
#include "ax900_profile.h"
#include "nvs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned char *blobs[4];static size_t sizes[4];static uint8_t last;
static int writes,fail_write;static uint8_t autos[4]={1,1,1,1};
esp_err_t nvs_flash_init(void){return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"ax900_wifi"));(void)mode;*h=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
static int slot(const char *key){assert(strlen(key)==5 && (!strncmp(key,"wifi",4) || !strncmp(key,"auto",4)) && key[4]>='0' && key[4]<='3');return key[4]-'0';}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *p,size_t *n){
    assert(h==1);int i=slot(key);if(!blobs[i])return ESP_ERR_NOT_FOUND;
    if(!p){*n=sizes[i];return ESP_OK;}assert(*n>=sizes[i]);*n=sizes[i];memcpy(p,blobs[i],*n);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *p,size_t n){
    assert(h==1);if(fail_write)return ESP_FAIL;int i=slot(key);free(blobs[i]);blobs[i]=malloc(n);memcpy(blobs[i],p,n);sizes[i]=n;writes++;return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *p){assert(h==1);*p=!strcmp(key,"last")?last:autos[slot(key)];return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t p){assert(h==1);if(!strcmp(key,"last"))last=p;else autos[slot(key)]=p;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);return ESP_OK;}
esp_err_t nvs_erase_all(nvs_handle_t h){assert(h==1);for(int i=0;i<4;i++){free(blobs[i]);blobs[i]=NULL;sizes[i]=0;}return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key){assert(h==1);int i=slot(key);if(!strncmp(key,"auto",4)){autos[i]=1;return ESP_OK;}if(!blobs[i])return ESP_ERR_NOT_FOUND;free(blobs[i]);blobs[i]=NULL;sizes[i]=0;return ESP_OK;}
static ax900_ap_t ap(unsigned id){ax900_ap_t a={0};a.ssid_len=1;a.raw_ssid[0]=id;a.secured=true;a.enterprise=true;a.frequency=5260;a.rssi=-50;return a;}
int main(void){
    assert(ax_profile_init()==ESP_OK);assert(ax_profile_count()==0);
    ax_connect_request_t *r=calloc(1,sizeof(*r));r->ap=ap(1);r->enterprise=true;r->allow_unverified_server=true;
    strcpy(r->username,"synthetic-user");strcpy(r->password,"synthetic-password");
    assert(ax_profile_save(r)==ESP_OK && ax_profile_count()==1);int saved_writes=writes;
    assert(ax_profile_save(r)==ESP_OK && writes==saved_writes); // no flash wear on repeated DHCP
    ax_connect_request_t *copy=ax_profile_find(&r->ap,1);assert(copy && !strcmp(copy->password,r->password));free(copy);
    strcpy(r->password,"new-synthetic-value");fail_write=1;assert(ax_profile_save(r)==ESP_FAIL);fail_write=0;
    copy=ax_profile_find(&r->ap,1);assert(copy && strcmp(copy->password,r->password));free(copy);
    ax900_ap_t changed=r->ap;changed.enterprise=false;changed.wpa2_psk=true;
    assert(!ax_profile_find(&changed,1));changed=r->ap;changed.secured=false;assert(!ax_profile_find(&changed,1));
    changed=r->ap;changed.pmf_required=true;assert(!ax_profile_find(&changed,1));
    r->association_test=true;assert(ax_profile_save(r)==ESP_ERR_INVALID_ARG);r->association_test=false;
    assert(ax_profile_save(r)==ESP_OK && ax_profile_count()==1);
    strcpy(r->ca_pem,"synthetic-ca");strcpy(r->server_name,"auth.example.invalid");r->allow_unverified_server=false;
    assert(ax_profile_save(r)==ESP_OK);copy=ax_profile_find(&r->ap,1);
    assert(copy && !copy->allow_unverified_server && !strcmp(copy->ca_pem,r->ca_pem) && !strcmp(copy->server_name,r->server_name));free(copy);
    for(int i=2;i<=5;i++){r->ap=ap(i);assert(ax_profile_save(r)==ESP_OK);}
    assert(ax_profile_count()==4);changed=ap(1);assert(!ax_profile_find(&changed,1));
    ax900_ap_t choices[]={ap(2),ap(5),ap(5)};choices[1].frequency=2412;choices[1].rssi=-20;
    copy=ax_profile_find(choices,3);assert(copy && copy->ap.raw_ssid[0]==5 && copy->ap.frequency==5260);free(copy);
    ax900_saved_network_t metadata[4];assert(ax900_list_saved(metadata,4)==4);
    assert(ax_profile_set_auto(&choices[2],false)==ESP_OK && !ax_profile_auto_enabled(&choices[2]));
    assert(ax_profile_auto_count()==3);
    copy=ax_profile_find(choices+2,1);assert(copy);free(copy);
    assert(!ax_profile_find_auto(choices+2,1));
    assert(ax_profile_set_auto(&choices[2],true)==ESP_OK);
    assert(ax_profile_auto_count()==4);
    copy=ax_profile_find_auto(choices+2,1);assert(copy);free(copy);
    ax900_ap_t remove=ap(2);assert(ax_profile_forget_network(&remove)==ESP_OK && ax_profile_count()==3);
    assert(!ax_profile_find(&remove,1));r->ap=ap(2);assert(ax_profile_save(r)==ESP_OK);
    // Unknown record version and oversized blobs must be ignored.
    blobs[last][0]=0xff;assert(ax_profile_count()==3);sizes[last]=100000;assert(ax_profile_count()==3);
    assert(ax_profile_forget()==ESP_OK && ax_profile_count()==0 && !ax_profile_find(choices,3));free(r);
    puts("Wi-Fi profiles: restore, update, bounded slots, security matching, CA policy, write failure, corruption and forget passed");
    return 0;
}
