// SPDX-License-Identifier: Apache-2.0
#include "ax900_internal.h"
#include "aic8800_protocol.h"
#include "ax900_rx.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "includes.h"
#include "common.h"
#include "rsn_supp/wpa.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "eap_peer/eap.h"
#include "eap_peer/eap_config.h"
#include "eap_peer/eap_methods.h"
#include "crypto/sha1.h"
#include "mbedtls/platform_util.h"
#include <stdatomic.h>
#include "ax900_profile.h"

struct packet { uint32_t generation;size_t len;bool encrypted;uint8_t bytes[1518]; };
static struct {
    ax900_device_t *d;
    esp_netif_t *netif;
    QueueHandle_t rx,tx;
    struct wpa_sm *sm;
    struct eapol_sm *eapol;
    struct eap_peer_config eap_config;
    struct wpa_config_blob ca_blob;
    ax_connect_request_t *credentials;
    enum wpa_states wpa_state;
    ax900_ap_t ap;
    uint8_t keys[5];
    atomic_bool authenticated, has_ip, ip_lost;
    atomic_uint generation;
    bool authorize, failed, recycled, save_attempted;
    uint16_t reason;
    int64_t deadline, dhcp_deadline;
    unsigned dhcp_retries;
} net;

void ax_parse_security(ax900_ap_t *ap) {
    ap->enterprise=ap->wpa2_psk=ap->sae=ap->pmf_required=false;
    struct wpa_ie_data ie;
    if(!ap->rsn_len || wpa_parse_wpa_ie_rsn(ap->rsn,ap->rsn_len,&ie))return;
    ap->enterprise=(ie.key_mgmt&WPA_KEY_MGMT_IEEE8021X) && (ie.pairwise_cipher&WPA_CIPHER_CCMP) && ie.group_cipher==WPA_CIPHER_CCMP;
    ap->sae=(ie.key_mgmt&WPA_KEY_MGMT_SAE)!=0;
    ap->pmf_required=(ie.capabilities&WPA_CAPABILITY_MFPR)!=0;
    ap->wpa2_psk=(ie.key_mgmt&WPA_KEY_MGMT_PSK) && (ie.pairwise_cipher&WPA_CIPHER_CCMP) && ie.group_cipher==WPA_CIPHER_CCMP;
}
static esp_err_t network_tx(void *handle,void *data,size_t length) {
    uint32_t generation=atomic_load(&net.generation);
    if(!net.authenticated || !net.tx || length<14 || length>1518)return ESP_ERR_INVALID_STATE;
    struct packet *p=malloc(sizeof(*p));if(!p)return ESP_ERR_NO_MEM;
    p->generation=generation;p->len=length;memcpy(p->bytes,data,length);
    if(xQueueSend(net.tx,&p,0)!=pdTRUE){free(p);return ESP_ERR_NO_MEM;}
    return ESP_OK;
}
static void free_rx(void *handle,void *buffer) {free(buffer);}
static void ip_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    ip_event_got_ip_t *event=data;
    if(!event || event->esp_netif!=net.netif)return;
    if(id==IP_EVENT_ETH_LOST_IP){net.has_ip=false;ax_ip_state(NULL);if(net.authenticated)net.ip_lost=true;return;}
    if(id==IP_EVENT_ETH_GOT_IP && net.authenticated) {
        net.has_ip=true;
        char ip[16];snprintf(ip,sizeof(ip),IPSTR,IP2STR(&event->ip_info.ip));ax_ip_state(ip);
        char status[96];snprintf(status,sizeof(status),"Connected: %s",ip);ax_status(status);
    }
}
esp_err_t ax_net_init(ax900_device_t *d) {
    if(!net.netif) {
        TRY(esp_netif_init());
        esp_err_t e=esp_event_loop_create_default();if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE)return e;
        esp_netif_inherent_config_t base=ESP_NETIF_INHERENT_DEFAULT_ETH();
        base.if_key="AX900";base.if_desc="AX900 USB Wi-Fi";base.route_prio=90;
        esp_netif_driver_ifconfig_t driver={.handle=&net,.transmit=network_tx,.driver_free_rx_buffer=free_rx};
        esp_netif_config_t cfg={.base=&base,.driver=&driver,.stack=ESP_NETIF_NETSTACK_DEFAULT_ETH};
        net.rx=xQueueCreate(24,sizeof(struct packet *));net.tx=xQueueCreate(24,sizeof(struct packet *));
        net.netif=esp_netif_new(&cfg);
        e=(!net.netif || !net.rx || !net.tx)?ESP_ERR_NO_MEM:ESP_OK;
        if(e==ESP_OK)e=esp_event_handler_register(IP_EVENT,IP_EVENT_ETH_GOT_IP,ip_event,NULL);
        if(e==ESP_OK)e=esp_event_handler_register(IP_EVENT,IP_EVENT_ETH_LOST_IP,ip_event,NULL);
        if(e!=ESP_OK){
            esp_event_handler_unregister(IP_EVENT,IP_EVENT_ETH_GOT_IP,ip_event);
            esp_event_handler_unregister(IP_EVENT,IP_EVENT_ETH_LOST_IP,ip_event);
            if(net.netif)esp_netif_destroy(net.netif);
            if(net.rx)vQueueDelete(net.rx);if(net.tx)vQueueDelete(net.tx);
            net.netif=NULL;net.rx=net.tx=NULL;return e;
        }
    }
    net.d=d;memset(net.keys,0xff,sizeof(net.keys));net.recycled=false;
    TRY(esp_netif_set_mac(net.netif,d->mac));
    esp_netif_action_start(net.netif,NULL,0,NULL);
    return ESP_OK;
}
static void drain(QueueHandle_t q) {struct packet *p;while(q && xQueueReceive(q,&p,0)==pdTRUE)free(p);}
static void clear_link(void) {
    net.save_attempted=false;net.ip_lost=false;net.dhcp_retries=0;net.failed=false;
    net.authenticated=false;net.has_ip=false;net.dhcp_deadline=0;atomic_fetch_add(&net.generation,1);net.authorize=false;net.deadline=0;
    if(net.netif)esp_netif_action_disconnected(net.netif,NULL,0,NULL);
    if(net.sm){wpa_sm_deinit(net.sm);net.sm=NULL;}
    if(net.eapol){eapol_sm_deinit(net.eapol);net.eapol=NULL;}
    ax_free_connect_request(net.credentials);net.credentials=NULL;
    memset(&net.eap_config,0,sizeof(net.eap_config));
    net.wpa_state=WPA_DISCONNECTED;
    drain(net.rx);drain(net.tx);
    ax_link_state(false,false,false,NULL,net.reason);
    if(net.d){net.d->associated=false;net.d->station=0xff;net.d->link_event=false;net.d->disconnected=false;}
}
void ax_net_stop(ax900_device_t *d) {
    if(net.d!=d)return;
    clear_link();esp_netif_action_stop(net.netif,NULL,0,NULL);net.d=NULL;
}
void ax_net_disconnect(ax900_device_t *d,uint16_t reason) {
    net.reason=reason;
    if(!d->gone) {
        uint8_t request[4]={0};put16(request,reason);request[2]=d->vif;
        (void)ax_command(d,AIC_SM_DISCONNECT_REQ,AIC_SM_DISCONNECT_CFM,request,sizeof(request),NULL,0,NULL);
    }
    d->associated=false;d->station=0xff;d->link_event=false;d->disconnected=false;clear_link();ax_status("Disconnected");
}
static void wpa_state(void *ctx,enum wpa_states state) {
    net.wpa_state=state;ESP_LOGI("AX900","WPA state=%d",state);
    if(state==WPA_COMPLETED)net.authorize=true;
}
static enum wpa_states get_wpa_state(void *ctx){return net.wpa_state;}
static void deauth(void *ctx,u16 reason){net.failed=true;net.reason=reason;}
static void reconnect(void *ctx){net.failed=true;net.reason=1;}
static void *network_context(void *ctx){return &net;}
static int get_bssid(void *ctx,u8 *bssid){memcpy(bssid,net.d->bssid,6);return 0;}
static int get_beacon(void *ctx){return wpa_sm_set_ap_rsn_ie(net.sm,net.ap.rsn,net.ap.rsn_len);}
static void cancel_auth(void *ctx){net.deadline=0;}
static int protection(void *ctx,const u8 *addr,int protect,int type){return 0;}
static int add_pmkid(void *ctx,void *network,const u8 *bssid,const u8 *pmkid,const u8 *cache,const u8 *pmk,size_t len,u32 lifetime,u8 threshold,int akmp){return 0;}
static int remove_pmkid(void *ctx,void *network,const u8 *bssid,const u8 *pmkid,const u8 *cache){return 0;}
static u8 *alloc_eapol(void *ctx,u8 type,const void *data,u16 len,size_t *total,void **body) {
    u8 *p=calloc(1,4+len);if(!p)return NULL;
    p[0]=2;p[1]=type;p[2]=len>>8;p[3]=len;*total=4+len;*body=p+4;if(data)memcpy(p+4,data,len);return p;
}
static int ether_send(void *ctx,const u8 *dest,u16 proto,const u8 *data,size_t length) {
    if(length>1500)return -1;
    uint8_t *p=malloc(14+length);if(!p)return -1;
    memcpy(p,dest,6);memcpy(p+6,net.d->mac,6);p[12]=proto>>8;p[13]=proto;
    memcpy(p+14,data,length);esp_err_t e=ax_data_tx(net.d,p,14+length);free(p);
    return e==ESP_OK?0:-1;
}
static int set_key(void *ctx,int link,enum wpa_alg alg,const u8 *addr,int index,int set_tx,const u8 *seq,size_t seq_len,const u8 *key,size_t len,enum key_flag flag) {
    bool pairwise=addr && !(addr[0]&1);unsigned slot=pairwise?4:index;
    if(slot>=5)return -1;
    if(net.keys[slot]!=0xff) {
        if(ax_command(net.d,AIC_MM_KEY_DEL_REQ,AIC_MM_KEY_DEL_CFM,&net.keys[slot],1,NULL,0,NULL)!=ESP_OK)return -1;
        net.keys[slot]=0xff;
    }
    if(alg==WPA_ALG_NONE)return 0;
    if(alg!=WPA_ALG_CCMP || len!=16 || index<0 || index>3)return -1;
    struct aic_wire_mm_key_add_req req={0};
    req.key_index=index;req.station_index=pairwise?net.d->station:0xff;
    req.key.length=len;memcpy(req.key.array,key,len);req.cipher=2;req.vif_index=net.d->vif;req.pairwise=pairwise;
    struct aic_wire_mm_key_add_cfm reply;size_t got=0;
    esp_err_t e=ax_command(net.d,AIC_MM_KEY_ADD_REQ,AIC_MM_KEY_ADD_CFM,&req,sizeof(req),&reply,sizeof(reply),&got);
    mbedtls_platform_zeroize(&req,sizeof(req));
    if(e!=ESP_OK || got<2 || reply.status)return -1;
    net.keys[slot]=reply.hardware_key_index;ESP_LOGI("AX900","Installed %s key",pairwise?"pairwise":"group");return 0;
}
static int eap_send(void *ctx,int type,const u8 *data,size_t length) {
    if(!net.d || !net.d->associated || length>1496)return -1;
    size_t total;void *body;u8 *frame=alloc_eapol(ctx,type,data,length,&total,&body);
    if(!frame)return -1;
    int result=ether_send(ctx,net.d->bssid,ETH_P_EAPOL,frame,total);free(frame);return result;
}
static const struct wpa_config_blob *eap_blob(void *ctx,const char *name) {
    return !strcmp(name,"ax900-ca")?&net.ca_blob:NULL;
}
static void eap_result(struct eapol_sm *eapol,enum eapol_supp_result result,void *ctx) {
    if(result==EAPOL_SUPP_RESULT_FAILURE){net.failed=true;net.reason=23;}
    else if(result==EAPOL_SUPP_RESULT_SUCCESS)ax_status("PEAP accepted; completing WPA2 keys");
}
static void eap_error(void *ctx,int code) {
    ESP_LOGW("AX900","EAP method error=%d",code);net.failed=true;net.reason=23;
}
static bool encryption_required(void *ctx){return net.authenticated;}
static void eap_status(void *ctx,const char *status,const char *parameter) {
    // Log only the state label; identity and method parameters can contain secrets.
    if(!strcmp(status,"started"))ax_status("PEAP authentication started");
    else if(!strcmp(status,"accept proposed method"))ax_status("Negotiating PEAP / MSCHAPv2");
    else if(!strcmp(status,"completion"))ESP_LOGI("AX900","EAP method completed");
}
static esp_err_t setup_eap(void) {
    static bool registered;
    static struct eap_method_type methods[]={{EAP_VENDOR_IETF,EAP_TYPE_PEAP},{EAP_VENDOR_IETF,EAP_TYPE_NONE}};
    if(!registered){
        if((eap_peer_peap_register() || eap_peer_mschapv2_register())){eap_peer_unregister_methods();return ESP_FAIL;}
        registered=true;
    }
    ax_connect_request_t *r=net.credentials;
    struct eap_peer_config *c=&net.eap_config;
    c->identity=(u8 *)r->username;c->identity_len=strlen(r->username);
    c->password=(u8 *)r->password;c->password_len=strlen(r->password);
    c->eap_methods=methods;c->phase2="auth=MSCHAPV2";c->fragment_size=1300;
    c->phase1="tls_disable_tlsv1_0=1 tls_disable_tlsv1_1=1 tls_disable_tlsv1_3=1";
    if(r->ca_pem[0]){
        net.ca_blob=(struct wpa_config_blob){.name="ax900-ca",.data=(u8 *)r->ca_pem,.len=strlen(r->ca_pem)+1};
        c->cert.ca_cert="blob://ax900-ca";c->cert.domain_match=r->server_name;
    } else if(!r->allow_unverified_server)return ESP_ERR_INVALID_ARG;
    struct eapol_ctx *ctx=calloc(1,sizeof(*ctx));if(!ctx)return ESP_ERR_NO_MEM;
    ctx->ctx=&net;ctx->cb_ctx=&net;ctx->eapol_send_ctx=&net;
    ctx->eapol_send=eap_send;ctx->get_config_blob=eap_blob;ctx->cb=eap_result;
    ctx->eap_error_cb=eap_error;ctx->status_cb=eap_status;ctx->encryption_required=encryption_required;
    net.eapol=eapol_sm_init(ctx);if(!net.eapol){free(ctx);return ESP_ERR_NO_MEM;}
    struct eapol_config conf={.accept_802_1x_keys=0,.fast_reauth=0,.workaround=1};
    eapol_sm_notify_config(net.eapol,c,&conf);
    eapol_sm_configure(net.eapol,10,60,2,3);
    wpa_sm_set_eapol(net.sm,net.eapol);
    return ESP_OK;
}
// Refresh firmware BSS state after scan completion or VIF recycling. The
// selected network is probed on its known channel before SM_CONNECT_REQ.
static esp_err_t confirm_target(ax900_device_t *d,const ax900_ap_t *ap) {
    struct aic_wire_scanu_start_req scan={0};
    memcpy(&scan.bssid,ap->bssid,6);scan.vif_index=d->vif;scan.channel_count=1;scan.ssid_count=1;
    scan.ssids[0].length=ap->ssid_len;memcpy(scan.ssids[0].array,ap->raw_ssid,ap->ssid_len);
    scan.channels[0].frequency=ap->frequency;scan.channels[0].band=ap->frequency>5000;scan.channels[0].tx_power=15;
    d->scan_done=false;d->scan_result=0xff;
    ax_status("Confirming target access point");
    // Acceptance payload is not a status byte; use the final scan confirmation.
    TRY(ax_command(d,AIC_SCANU_START_REQ,AIC_SCANU_START_ACCEPTED,&scan,sizeof(scan),NULL,0,NULL));
    int64_t until=esp_timer_get_time()+3000000;
    while(!d->scan_done && !d->gone && !d->fault && esp_timer_get_time()<until)ax_pump(10);
    if(!d->scan_done)return ESP_ERR_TIMEOUT;
    if(d->scan_result)return ESP_FAIL;
    for(unsigned i=0;i<5;i++)ax_pump(10);
    return ESP_OK;
}
static esp_err_t connect_request(ax900_device_t *d,ax_connect_request_t *request) {
    if(net.d!=d){ax_free_connect_request(request);return ESP_ERR_INVALID_STATE;}
    clear_link();net.credentials=request;
    const ax900_ap_t *ap=&request->ap;const char *password=request->password;
    ax_link_state(true,false,false,ap->ssid,0);net.ap=*ap;net.failed=false;net.reason=0;
    // Recycle after an established session, preserving the initial scan cache before first association.
    if(net.recycled) {
        TRY(ax_command(d,AIC_MM_REMOVE_IF_REQ,AIC_MM_REMOVE_IF_CFM,&d->vif,1,NULL,0,NULL));
        uint8_t add[10]={0},reply[2];size_t got;memcpy(add+2,d->mac,6);
        TRY(ax_command(d,AIC_MM_ADD_IF_REQ,AIC_MM_ADD_IF_CFM,add,sizeof(add),reply,sizeof(reply),&got));
        if(got!=2 || reply[0])return ESP_FAIL;d->vif=reply[1];net.recycled=false;
    }
    memcpy(d->bssid,ap->bssid,6);
    memset(net.keys,0xff,sizeof(net.keys));d->station=0xff;d->link_event=false;d->disconnected=false;d->associated=false;
    TRY(confirm_target(d,ap));
    struct aic_wire_sm_connect_req req={0};
    req.ssid.length=ap->ssid_len;memcpy(req.ssid.array,ap->raw_ssid,ap->ssid_len);
    memcpy(&req.bssid,ap->bssid,6);req.channel.frequency=ap->frequency;
    req.channel.band=ap->frequency>5000;req.channel.tx_power=15;req.vif_index=d->vif;req.uapsd_queues=1;
    ((uint8_t *)&req.control_port_ethertype)[0]=0x88;((uint8_t *)&req.control_port_ethertype)[1]=0x8e;
    if(ap->secured) {
        if((request->enterprise?!ap->enterprise:!ap->wpa2_psk) || ap->pmf_required)return ESP_ERR_NOT_SUPPORTED;
        struct wpa_sm_ctx *ctx=calloc(1,sizeof(*ctx));if(!ctx)return ESP_ERR_NO_MEM;
        ctx->ctx=&net;ctx->set_state=wpa_state;ctx->get_state=get_wpa_state;ctx->deauthenticate=deauth;ctx->reconnect=reconnect;
        ctx->set_key=set_key;ctx->get_network_ctx=network_context;ctx->get_bssid=get_bssid;ctx->ether_send=ether_send;
        ctx->get_beacon_ie=get_beacon;ctx->cancel_auth_timeout=cancel_auth;ctx->alloc_eapol=alloc_eapol;
        ctx->add_pmkid=add_pmkid;ctx->remove_pmkid=remove_pmkid;ctx->mlme_setprotection=protection;
        net.sm=wpa_sm_init(ctx);if(!net.sm){free(ctx);return ESP_ERR_NO_MEM;}
        wpa_sm_set_own_addr(net.sm,d->mac);
        struct rsn_supp_config conf={.network_ctx=&net,.allowed_pairwise_cipher=WPA_CIPHER_CCMP,.ssid=net.ap.raw_ssid,.ssid_len=net.ap.ssid_len};
        wpa_sm_set_config(net.sm,&conf);
        wpa_sm_set_param(net.sm,WPA_PARAM_PROTO,WPA_PROTO_RSN);
        wpa_sm_set_param(net.sm,WPA_PARAM_PAIRWISE,WPA_CIPHER_CCMP);
        wpa_sm_set_param(net.sm,WPA_PARAM_GROUP,WPA_CIPHER_CCMP);
        wpa_sm_set_param(net.sm,WPA_PARAM_KEY_MGMT,request->enterprise?WPA_KEY_MGMT_IEEE8021X:WPA_KEY_MGMT_PSK);
        wpa_sm_set_param(net.sm,WPA_PARAM_RSN_ENABLED,1);
        if(request->enterprise && !request->association_test){TRY(setup_eap());}
        else if(!request->association_test) {
        uint8_t pmk[32];int result;
        if(strlen(password)==64)result=hexstr2bin(password,pmk,sizeof(pmk));
        else result=pbkdf2_sha1(password,ap->raw_ssid,ap->ssid_len,4096,pmk,sizeof(pmk));
        if(result){mbedtls_platform_zeroize(pmk,sizeof(pmk));return ESP_ERR_INVALID_ARG;}
        wpa_sm_set_pmk(net.sm,pmk,sizeof(pmk),NULL,NULL);mbedtls_platform_zeroize(pmk,sizeof(pmk));
        }
        if(wpa_sm_set_ap_rsn_ie(net.sm,ap->rsn,ap->rsn_len))return ESP_FAIL;
        size_t ie_len=sizeof(req.ie_buffer);
        if(wpa_sm_set_assoc_wpa_ie_default(net.sm,(uint8_t *)req.ie_buffer,&ie_len))return ESP_FAIL;
        req.ie_length=ie_len;req.flags=1|2|8;
    }
    ESP_LOGI("AX900","Connect frequency=%u vif=%u flags=%lu RSN=%u diagnostic=%u",ap->frequency,d->vif,(unsigned long)req.flags,req.ie_length,request->association_test);
    ax_link_state(true,false,false,ap->ssid,0);ax_status("Associating with access point");
    uint8_t reply[4];size_t got=0;
    esp_err_t e=ax_command(d,AIC_SM_CONNECT_REQ,AIC_SM_CONNECT_CFM,&req,sizeof(req),reply,sizeof(reply),&got);
    if(e!=ESP_OK || got<1 || reply[0]){clear_link();return e==ESP_OK?ESP_FAIL:e;}
    net.deadline=esp_timer_get_time()+(request->enterprise?90000000:30000000);return ESP_OK;
}
esp_err_t ax_net_connect(ax900_device_t *d,ax_connect_request_t *request) {
    esp_err_t result=connect_request(d,request);
    if(result!=ESP_OK && net.d==d)clear_link();
    return result;
}
static void enqueue_rx(void *ctx,const uint8_t *data,size_t length,bool encrypted) {
    if(length>1518 || !net.rx)return;
    struct packet *p=malloc(sizeof(*p));if(!p){ax_packet_count(false,true);return;}
    p->len=length;p->encrypted=encrypted;memcpy(p->bytes,data,length);
    if(xQueueSend(net.rx,&p,0)!=pdTRUE){free(p);ax_packet_count(false,true);}
}
void ax_net_receive(void *arg,const uint8_t *record,size_t length) {
    ax900_device_t *d=arg;
    if(d!=net.d)return;
    if(length>=90 && (record[48]&2)) {
        const uint8_t *f=record+60;unsigned subtype=f[0]>>4;
        if(!memcmp(f+10,d->bssid,6) && (subtype==1 || subtype==11 || subtype==12))
            ESP_LOGI("AX900","Management subtype=%u status=%u",subtype,get16(f+(subtype==11?28:subtype==1?26:24)));
    }
    if(!d->associated)return;
    if(!ax_decode_rx(record,length,d->vif,d->bssid,enqueue_rx,NULL))ax_packet_count(false,true);
}
void ax_net_poll(ax900_device_t *d) {
    if(d!=net.d || d->fault || d->gone || d->stopping)return;
    if(d->disconnected){
        bool session=net.credentials!=NULL;
        bool diagnostic=session && net.credentials->association_test;
        net.reason=d->disconnect_reason;
        // A rejection during authentication can indicate stale credentials.
        bool auth_failure=session && (!net.authenticated || (net.reason>=14 && net.reason<=24));
        clear_link();ax_status("Access point disconnected");
        if(session && !diagnostic)ax_reconnect_lost(auth_failure);
        return;
    }
    if(d->link_event) {
        d->link_event=false;
        if(d->link_status || !d->associated){net.reason=d->link_status;clear_link();ax_status("Association failed (IEEE status in reason)");ax_reconnect_lost(false);return;}
        else {
            net.recycled=true; // Recycle only after a real association has created peer state.
            ax_link_state(true,true,false,net.ap.ssid,0);
            if(net.credentials && net.credentials->association_test){
                net.deadline=esp_timer_get_time()+10000000;ax_status("Association test passed; no credentials sent");
            } else if(net.sm){net.wpa_state=WPA_ASSOCIATED;wpa_sm_notify_assoc(net.sm,d->bssid);
                if(net.eapol){eapol_sm_notify_portValid(net.eapol,false);eapol_sm_notify_portEnabled(net.eapol,true);}
                ax_status(net.eapol?"Associated; starting PEAP":"Associated; authenticating WPA2");}
            else net.authorize=true;
        }
    }
    struct packet *p;
    for(unsigned budget=0;budget<24 && !d->fault && !d->gone && xQueueReceive(net.rx,&p,0)==pdTRUE;budget++) {
        ax_packet_count(false,false);
        bool eapol=p->len>=14 && p->bytes[12]==0x88 && p->bytes[13]==0x8e;
        if(eapol && net.sm && !memcmp(p->bytes+6,d->bssid,6)) {
            ESP_LOGI("AX900","EAPOL received (%u bytes)",(unsigned)p->len);
            enum frame_encryption encryption=p->encrypted?FRAME_ENCRYPTED:FRAME_NOT_ENCRYPTED;
            if(net.eapol)eapol_sm_rx_eapol(net.eapol,p->bytes+6,p->bytes+14,p->len-14,encryption);
            if(p->len>=18 && p->bytes[15]==IEEE802_1X_TYPE_EAPOL_KEY)
                wpa_sm_rx_eapol(net.sm,p->bytes+6,p->bytes+14,p->len-14,encryption);
        } else if(net.authenticated && !eapol) {
            void *copy=malloc(p->len);
            if(copy){memcpy(copy,p->bytes,p->len);(void)esp_netif_receive(net.netif,copy,p->len,NULL);}
        }
        free(p);
    }
    if(d->fault || d->gone)return;
    ax_supplicant_poll();
    if(d->fault || d->gone)return;
    if(net.eapol && eapol_sm_failed(net.eapol)){net.failed=true;net.reason=23;}
    if(net.failed || (net.deadline && esp_timer_get_time()>net.deadline)) {
        bool diagnostic=net.credentials && net.credentials->association_test;
        bool timeout=!net.failed;ax_net_disconnect(d,net.reason?net.reason:15);net.failed=false;
        ax_status(diagnostic?"Association test ended":timeout?"Connection timed out":"WPA2 authentication failed");
        if(!diagnostic)ax_reconnect_lost(true);
        return;
    }
    if(net.authorize && d->associated) {
        net.authorize=false;uint8_t req[2]={d->station,1};
        if(ax_command(d,AIC_ME_CONTROL_PORT_REQ,AIC_ME_CONTROL_PORT_CFM,req,sizeof(req),NULL,0,NULL)==ESP_OK) {
            net.authenticated=true;net.deadline=0;net.dhcp_retries=0;net.dhcp_deadline=esp_timer_get_time()+30000000;ax_link_state(false,true,true,net.ap.ssid,0);
            ax_status("Authenticated; requesting DHCP address");esp_netif_action_connected(net.netif,NULL,0,NULL);
        } else {net.failed=true;net.reason=1;}
    }
    if(d->fault || d->gone)return;
    if(atomic_exchange(&net.ip_lost,false) && net.authenticated && !net.has_ip){
        net.dhcp_deadline=esp_timer_get_time()+30000000;net.dhcp_retries=0;
        ax_status("IP address lost; waiting for DHCP");
    }
    if(net.authenticated && !net.has_ip && net.dhcp_deadline && esp_timer_get_time()>net.dhcp_deadline){
        if(net.dhcp_retries++<2){
            net.dhcp_deadline=esp_timer_get_time()+30000000;
            (void)esp_netif_dhcpc_stop(net.netif);(void)esp_netif_dhcpc_start(net.netif);
            ax_status("DHCP timeout; retrying address request");
        } else {
            ax_net_disconnect(d,3);ax_status("DHCP retry limit reached");ax_reconnect_lost(false);return;
        }
    }
    if(net.authenticated && net.has_ip && net.credentials && !net.credentials->skip_save && !net.save_attempted) {
        net.save_attempted=true;
        esp_err_t e=ax_profile_save(net.credentials);ax_profile_status(e==ESP_OK,e);
        ESP_LOGI("AX900","Wi-Fi profile save: %s",esp_err_to_name(e));
    }
    for(unsigned budget=0;budget<8 && !d->fault && !d->gone && xQueueReceive(net.tx,&p,0)==pdTRUE;budget++) {
        if(net.authenticated && p->generation==atomic_load(&net.generation))(void)ax_data_tx(d,p->bytes,p->len);free(p);
    }
}
void ax_net_disable_save(void){net.save_attempted=true;}
#if CONFIG_AX900_FAULT_INJECTION
void ax_net_debug_dhcp_timeout(void) {
    // Exercise the terminal DHCP timeout path without modifying router settings.
    net.has_ip=false;ax_ip_state(NULL);net.dhcp_retries=2;net.dhcp_deadline=esp_timer_get_time()-1;
}
#endif
