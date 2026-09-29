// SPDX-License-Identifier: Apache-2.0
// Real Hostap WPA2 four-way handshake with synthetic AP frames and credentials.
#include "includes.h"
#include "common.h"
#include "rsn_supp/wpa.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "crypto/sha1.h"
#include "crypto/aes_wrap.h"
#include <assert.h>
static const u8 station[6]={2,0,0,0,0,1},bssid[6]={2,0,0,0,0,2};
static const u8 rsn[]={48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
static const u8 rsnxe[]={244,1,0x20};
static u8 sent[512];static size_t sent_len;static enum wpa_states state;static unsigned installed;static u16 rejected;
int64_t esp_timer_get_time(void){return 1000000;}
static void set_state(void *ctx,enum wpa_states s){state=s;}
static enum wpa_states get_state(void *ctx){return state;}
static void deauth(void *ctx,u16 reason){if(!rejected)rejected=reason;}
static void no_op(void *ctx){}
static void *network(void *ctx){return &state;}
static int get_bssid(void *ctx,u8 *out){memcpy(out,bssid,6);return 0;}
static int set_key(void *ctx,int link,enum wpa_alg alg,const u8 *addr,int index,int tx,const u8 *seq,size_t sl,const u8 *key,size_t n,enum key_flag flag){
    assert(alg==WPA_ALG_CCMP && n==16);installed++;return 0;
}
static int transmit(void *ctx,const u8 *dest,u16 proto,const u8 *data,size_t len){assert(len<=sizeof(sent));memcpy(sent,data,len);sent_len=len;return 0;}
static u8 *alloc_eapol(void *ctx,u8 type,const void *data,u16 len,size_t *total,void **body){u8 *p=calloc(1,4+len);assert(p);p[0]=2;p[1]=type;WPA_PUT_BE16(p+2,len);*total=4+len;*body=p+4;if(data)memcpy(p+4,data,len);return p;}
static int protect(void *ctx,const u8 *addr,int a,int b){return 0;}
static void handshake(bool remember_rsnxe,bool wrong_key){
    state=WPA_DISCONNECTED;installed=0;rejected=0;sent_len=0;
    struct wpa_sm_ctx *ctx=calloc(1,sizeof(*ctx));assert(ctx);
    ctx->set_state=set_state;ctx->get_state=get_state;ctx->deauthenticate=deauth;ctx->reconnect=no_op;
    ctx->set_key=set_key;ctx->get_network_ctx=network;ctx->get_bssid=get_bssid;ctx->ether_send=transmit;
    ctx->cancel_auth_timeout=no_op;ctx->alloc_eapol=alloc_eapol;ctx->mlme_setprotection=protect;
    struct wpa_sm *sm=wpa_sm_init(ctx);assert(sm);wpa_sm_set_own_addr(sm,station);
    struct rsn_supp_config conf={.network_ctx=&state,.allowed_pairwise_cipher=WPA_CIPHER_CCMP,.ssid=(u8 *)"IEEE",.ssid_len=4};
    wpa_sm_set_config(sm,&conf);wpa_sm_set_param(sm,WPA_PARAM_PROTO,WPA_PROTO_RSN);
    wpa_sm_set_param(sm,WPA_PARAM_PAIRWISE,WPA_CIPHER_CCMP);wpa_sm_set_param(sm,WPA_PARAM_GROUP,WPA_CIPHER_CCMP);
    wpa_sm_set_param(sm,WPA_PARAM_KEY_MGMT,WPA_KEY_MGMT_PSK);wpa_sm_set_param(sm,WPA_PARAM_RSN_ENABLED,1);
    u8 pmk[32],expected[32];assert(!pbkdf2_sha1("password",(u8 *)"IEEE",4,4096,pmk,sizeof(pmk)));
    assert(!hexstr2bin("f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e",expected,sizeof(expected)));
    assert(!memcmp(pmk,expected,sizeof(pmk)));if(wrong_key)pmk[0]^=1;
    wpa_sm_set_pmk(sm,pmk,sizeof(pmk),NULL,NULL);assert(!wpa_sm_set_ap_rsn_ie(sm,rsn,sizeof(rsn)));
    if(remember_rsnxe)assert(!wpa_sm_set_ap_rsnxe(sm,rsnxe,sizeof(rsnxe)));
    u8 assoc[128];size_t assoc_len=sizeof(assoc);assert(!wpa_sm_set_assoc_wpa_ie_default(sm,assoc,&assoc_len));
    wpa_sm_notify_assoc(sm,bssid);
    u8 first[99]={2,3,0,95};struct wpa_eapol_key *m1=(void *)(first+4);m1->type=2;
    WPA_PUT_BE16(m1->key_info,2|WPA_KEY_INFO_KEY_TYPE|WPA_KEY_INFO_ACK);WPA_PUT_BE16(m1->key_length,16);
    m1->replay_counter[7]=1;memset(m1->key_nonce,0x33,32);
    wpa_sm_rx_eapol(sm,bssid,first,sizeof(first),FRAME_NOT_ENCRYPTED);assert(sent_len>=99);
    struct wpa_eapol_key *m2=(void *)(sent+4);struct wpa_ptk ptk={0};
    assert(!wpa_pmk_to_ptk(expected,sizeof(expected),"Pairwise key expansion",station,bssid,m2->key_nonce,m1->key_nonce,&ptk,WPA_KEY_MGMT_PSK,WPA_CIPHER_CCMP,NULL,0,0));
    u8 third[163]={2,3,0,159};struct wpa_eapol_key *m3=(void *)(third+4);m3->type=2;
    WPA_PUT_BE16(m3->key_info,2|WPA_KEY_INFO_KEY_TYPE|WPA_KEY_INFO_INSTALL|WPA_KEY_INFO_ACK|WPA_KEY_INFO_MIC|WPA_KEY_INFO_SECURE|WPA_KEY_INFO_ENCR_KEY_DATA);
    WPA_PUT_BE16(m3->key_length,16);m3->replay_counter[7]=2;memcpy(m3->key_nonce,m1->key_nonce,32);
    u8 data[56]={0};memcpy(data,rsn,sizeof(rsn));memcpy(data+sizeof(rsn),rsnxe,sizeof(rsnxe));
    size_t pos=sizeof(rsn)+sizeof(rsnxe);u8 gtk[]={221,22,0,15,172,1,1,0};memcpy(data+pos,gtk,sizeof(gtk));memset(data+pos+sizeof(gtk),0x55,16);data[pos+24]=221;
    WPA_PUT_BE16(third+97,64);assert(!aes_wrap(ptk.kek,ptk.kek_len,7,data,third+99));
    assert(!wpa_eapol_key_mic(ptk.kck,ptk.kck_len,WPA_KEY_MGMT_PSK,2,third,sizeof(third),third+81));
    wpa_sm_rx_eapol(sm,bssid,third,sizeof(third),FRAME_NOT_ENCRYPTED);
    if(wrong_key)assert(state!=WPA_COMPLETED && !installed);
    else if(remember_rsnxe)assert(state==WPA_COMPLETED && installed==2 && !rejected);
    else assert(state!=WPA_COMPLETED && rejected==WLAN_REASON_IE_IN_4WAY_DIFFERS && !installed);
    wpa_sm_deinit(sm);
}
int main(void){handshake(false,false);handshake(true,false);handshake(true,true);puts("WPA2 handshake: PBKDF2 vector, RSNXE regression, PTK/GTK installation and bad MIC rejection passed");}
