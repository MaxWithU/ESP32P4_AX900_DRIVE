// SPDX-License-Identifier: Apache-2.0
// ESP-IDF USB transport. Wire protocol adapted from the attributed RT-Smart driver.
#include "ax900_internal.h"
#include "ax900_frame.h"
#include "esp_timer.h"
#include "aic8800_protocol.h"
#include <stdlib.h>
#include <stdio.h>
#include "mbedtls/platform_util.h"
#include "ax900_profile.h"

static usb_host_client_handle_t client;
static ax900_device_t *active;
static bool pending[128];
static usb_device_handle_t opened[128];
static bool removed[128];
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static ax900_status_t state;
static bool scan_requested, connect_requested, disconnect_requested;
static ax_connect_request_t *requested_connection;
static bool forget_requested, auto_reconnect_pending=true;
void ax_profile_status(bool saved,esp_err_t error) {
    unsigned count=ax_profile_count();
    taskENTER_CRITICAL(&lock);
    state.saved_networks=count;state.credentials_saved=saved;state.profile_error=error;
    taskEXIT_CRITICAL(&lock);
}
void ax_parse_security(ax900_ap_t *ap);
void ax_link_state(bool connecting, bool associated, bool authenticated, const char *ssid, uint16_t reason) {
    taskENTER_CRITICAL(&lock);
    if((state.connecting || state.associated) && !connecting && !associated)state.connection_id++;
    state.connecting=connecting;state.associated=associated;state.authenticated=authenticated;
    state.disconnect_reason=reason;
    if(ssid)snprintf(state.connected_ssid,sizeof(state.connected_ssid),"%s",ssid);
    if(!authenticated){state.has_ip=false;state.ip[0]=0;state.credentials_saved=false;}
    taskEXIT_CRITICAL(&lock);
}
bool ax900_connection_is_current(uint32_t connection_id) {
    taskENTER_CRITICAL(&lock);
    bool current=state.connection_id==connection_id && state.ready && state.associated && state.authenticated && state.has_ip;
    taskEXIT_CRITICAL(&lock);
    return current;
}
void ax_ip_state(const char *ip) {
    taskENTER_CRITICAL(&lock);
    state.has_ip=ip && ip[0] && state.authenticated;
    snprintf(state.ip,sizeof(state.ip),"%s",state.has_ip?ip:"");
    taskEXIT_CRITICAL(&lock);
}
void ax_packet_count(bool tx,bool dropped) {
    taskENTER_CRITICAL(&lock);
    if(dropped)state.rx_dropped++;else if(tx)state.tx_packets++;else state.rx_packets++;
    taskEXIT_CRITICAL(&lock);
}
void ax_free_connect_request(ax_connect_request_t *request) {
    if(request){mbedtls_platform_zeroize(request,sizeof(*request));free(request);}
}
static esp_err_t queue_connect(ax_connect_request_t *request) {
    taskENTER_CRITICAL(&lock);
    bool ready=state.ready && !state.scanning && !scan_requested && !state.connecting && !state.associated && !connect_requested;
    if(ready){requested_connection=request;connect_requested=true;auto_reconnect_pending=false;state.connecting=true;state.connection_id++;state.connected_frequency=request->ap.frequency;}
    taskEXIT_CRITICAL(&lock);
    if(!ready)ax_free_connect_request(request);
    return ready?ESP_OK:ESP_ERR_INVALID_STATE;
}
static bool valid_ap(const ax900_ap_t *ap) {
    return ap && ap->ssid_len && ap->ssid_len<=32 && ap->rsn_len<=sizeof(ap->rsn) && !(ap->bssid[0]&1);
}
esp_err_t ax900_connect(const ax900_ap_t *ap,const char *password) {
    if(!valid_ap(ap) || !password)return ESP_ERR_INVALID_ARG;
    size_t len=strnlen(password,65);
    ax900_ap_t checked=*ap;ax_parse_security(&checked);
    if(len>64 || (ap->secured && (len<8 || !checked.wpa2_psk || checked.pmf_required)) || (!ap->secured && len))return ESP_ERR_NOT_SUPPORTED;
    ax_connect_request_t *request=calloc(1,sizeof(*request));if(!request)return ESP_ERR_NO_MEM;
    request->ap=checked;memcpy(request->password,password,len+1);
    return queue_connect(request);
}
esp_err_t ax900_connect_peap(const ax900_ap_t *ap,const ax900_peap_config_t *config) {
    if(!valid_ap(ap) || !config || !config->username || !config->password)return ESP_ERR_INVALID_ARG;
    size_t user_len=strnlen(config->username,129),pass_len=strnlen(config->password,129);
    size_t ca_len=config->ca_cert_pem?strnlen(config->ca_cert_pem,8193):0;
    size_t name_len=config->server_name?strnlen(config->server_name,254):0;
    if(!user_len || user_len>128 || !pass_len || pass_len>128 || ca_len>8192 || name_len>253)return ESP_ERR_INVALID_ARG;
    if((!ca_len || !name_len) && !config->allow_unverified_server)return ESP_ERR_INVALID_ARG;
    if((ca_len!=0)!=(name_len!=0))return ESP_ERR_INVALID_ARG;
    ax900_ap_t checked=*ap;ax_parse_security(&checked);
    if(!checked.enterprise || checked.pmf_required)return ESP_ERR_NOT_SUPPORTED;
    ax_connect_request_t *request=calloc(1,sizeof(*request));if(!request)return ESP_ERR_NO_MEM;
    request->ap=checked;request->enterprise=true;request->allow_unverified_server=config->allow_unverified_server;
    memcpy(request->username,config->username,user_len);memcpy(request->password,config->password,pass_len);
    if(ca_len)memcpy(request->ca_pem,config->ca_cert_pem,ca_len);
    if(name_len)memcpy(request->server_name,config->server_name,name_len);
    return queue_connect(request);
}
esp_err_t ax900_test_association(const ax900_ap_t *ap) {
    if(!valid_ap(ap))return ESP_ERR_INVALID_ARG;
    ax900_ap_t checked=*ap;ax_parse_security(&checked);
    if(checked.pmf_required || (checked.secured && !checked.enterprise && !checked.wpa2_psk))return ESP_ERR_NOT_SUPPORTED;
    ax_connect_request_t *request=calloc(1,sizeof(*request));if(!request)return ESP_ERR_NO_MEM;
    request->ap=checked;request->enterprise=checked.enterprise;request->association_test=true;
    return queue_connect(request);
}
esp_err_t ax900_disconnect(void) {
    taskENTER_CRITICAL(&lock);bool ready=state.ready;if(ready){disconnect_requested=true;auto_reconnect_pending=false;}taskEXIT_CRITICAL(&lock);
    return ready?ESP_OK:ESP_ERR_INVALID_STATE;
}
esp_err_t ax900_connect_saved(const ax900_ap_t *ap) {
    if(!valid_ap(ap))return ESP_ERR_INVALID_ARG;
    ax_connect_request_t *r=ax_profile_find(ap,1);
    return r?queue_connect(r):ESP_ERR_NOT_FOUND;
}
bool ax900_has_saved(const ax900_ap_t *ap) {
    if(!valid_ap(ap))return false;
    ax_connect_request_t *r=ax_profile_find(ap,1);bool found=r!=NULL;ax_free_connect_request(r);return found;
}
esp_err_t ax900_forget_saved(void) {
    if(!client)return ESP_ERR_INVALID_STATE;
    taskENTER_CRITICAL(&lock);forget_requested=true;auto_reconnect_pending=false;taskEXIT_CRITICAL(&lock);
    return ESP_OK;
}
static const char *TAG="AX900";

void ax_status(const char *text) {
    taskENTER_CRITICAL(&lock);
    snprintf(state.status,sizeof(state.status),"%s",text);
    taskEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,"%s",text);
}
void ax900_get_status(ax900_status_t *out) {
    if (!out) return;
    taskENTER_CRITICAL(&lock); *out=state; taskEXIT_CRITICAL(&lock);
}
void ax_set_ready(bool band5) {
    taskENTER_CRITICAL(&lock);
    state.ready=true;state.supports_5ghz=band5;
    taskEXIT_CRITICAL(&lock);
}
esp_err_t ax900_request_scan(void) {
    taskENTER_CRITICAL(&lock);
    esp_err_t e=state.ready && !state.scanning && !scan_requested && !state.connecting && !state.associated ? ESP_OK : ESP_ERR_INVALID_STATE;
    if(e==ESP_OK) scan_requested=true;
    taskEXIT_CRITICAL(&lock);
    return e;
}
void ax_pump(unsigned ms) {usb_host_client_handle_events(client,pdMS_TO_TICKS(ms));}

void ax_message(ax900_device_t *d,uint16_t id,const uint8_t *p,size_t n) {
    if(id==AIC_SM_CONNECT_IND && n>=14 && p[9]==d->vif) {
        d->link_status=get16(p);d->associated=d->link_status==0;d->link_event=true;
        if(d->associated){memcpy(d->bssid,p+2,6);d->station=p[10];d->qos=p[12]!=0;}
        ESP_LOGI(TAG,"Association result=%u station=%u QoS=%u",d->link_status,d->station,d->qos);
        if(n>=18)ESP_LOGI(TAG,"Association report bytes=%u request_IE=%u response_IE=%u",(unsigned)n,get16(p+14),get16(p+16));
    }
    if(id==AIC_SM_DISCONNECT_IND && n>=3 && p[2]==d->vif) {
        d->associated=false;d->disconnected=true;d->disconnect_reason=get16(p);
        ESP_LOGI(TAG,"Disconnected: reason=%u",d->disconnect_reason);
    }
    if(id==0x1001 && n>=3 && p[0]==d->vif) {
        d->scan_result=p[1];d->scan_done=true;
        ESP_LOGI(TAG,"SCAN_DONE status=%u firmware_results=%u",p[1],p[2]);
    }
    if(id!=0x1004 || n<48) return;
    taskENTER_CRITICAL(&lock);bool scanning=state.scanning;taskEXIT_CRITICAL(&lock);
    if(!scanning)return;
    size_t flen=get16(p);
    if(flen<36 || flen>n-12) return;
    ax900_ap_t ap={0};
    ap.frequency=get16(p+4);ap.rssi=(int8_t)p[9];
    const uint8_t *f=p+12;
    memcpy(ap.bssid,f+16,6);ap.secured=(get16(f+34)&0x10)!=0;
    for(size_t off=36;off+2<=flen;) {
        size_t len=f[off+1]; if(len>flen-off-2) return;
        if(f[off]==0 && len<=32) {memcpy(ap.ssid,f+off+2,len);memcpy(ap.raw_ssid,f+off+2,len);ap.ssid_len=len;}
        if(f[off]==48 && len+2<=sizeof(ap.rsn)){memcpy(ap.rsn,f+off,len+2);ap.rsn_len=len+2;}
        off+=2+len;
    }
    ax_parse_security(&ap);
    // SSIDs are arbitrary bytes; keep serial/UI output free of control characters.
    for(size_t i=0;i<32 && ap.ssid[i];i++) if((uint8_t)ap.ssid[i]<32 || ap.ssid[i]==127) ap.ssid[i]='?';
    taskENTER_CRITICAL(&lock);
    size_t i=0;for(;i<state.ap_count;i++) if(!memcmp(state.aps[i].bssid,ap.bssid,6)) break;
    if(i<AX900_MAX_APS) {state.aps[i]=ap;if(i==state.ap_count)state.ap_count++;}
    taskEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,"AP band=%s freq=%u RSSI=%d SSID=%s PSK=%u EAP=%u SAE=%u PMF-required=%u",ap.frequency>5000?"5GHz":"2.4GHz",ap.frequency,ap.rssi,ap.ssid,ap.wpa2_psk,ap.enterprise,ap.sae,ap.pmf_required);
}

static void dispatch_message(void *arg,uint16_t id,const uint8_t *p,size_t n) {
    ax900_device_t *d=arg;
    if(d->waiting_id && id==d->waiting_id && !d->reply_done) {
        d->reply_len=n;
        d->reply_error=n<=sizeof(d->reply)?ESP_OK:ESP_ERR_INVALID_SIZE;
        if(d->reply_error==ESP_OK)memcpy(d->reply,p,n);
        d->reply_done=true;
    } else ax_message(d,id,p,n);
}
static void receive_cb(usb_transfer_t *t) {
    ax900_device_t *d=t->context;d->rx_pending=false;
    if(t->status!=USB_TRANSFER_STATUS_COMPLETED) {
        if(!d->stopping && !d->gone) {d->reply_error=ESP_FAIL;d->reply_done=true;}
        return;
    }
    if(!ax_walk_records(t->data_buffer,t->actual_num_bytes,dispatch_message,ax_net_receive,d))
        ESP_LOGW(TAG,"Malformed USB record discarded");
    if(!d->stopping && !d->gone) {
        esp_err_t e=usb_host_transfer_submit(t);
        d->rx_pending=e==ESP_OK;
        if(e!=ESP_OK){d->reply_error=e;d->reply_done=true;}
    }
}
static void done_cb(usb_transfer_t *t) {*(bool *)t->context=true;}

static esp_err_t transfer(ax900_device_t *d,uint8_t ep,uint8_t *p,size_t n,size_t *got) {
    usb_transfer_t *t=NULL;TRY(usb_host_transfer_alloc(n,0,&t));
    bool done=false;
    t->device_handle=d->usb;t->bEndpointAddress=ep;t->num_bytes=n;
    t->callback=done_cb;t->context=&done;
    if(!(ep&0x80)){memcpy(t->data_buffer,p,n);t->flags=USB_TRANSFER_FLAG_ZERO_PACK;}
    esp_err_t e=usb_host_transfer_submit(t);
    if(e==ESP_OK) {
        int64_t until=esp_timer_get_time()+3000000;
        while(!done && esp_timer_get_time()<until)ax_pump(10);
        if(!done) {
            usb_host_endpoint_halt(d->usb,ep);usb_host_endpoint_flush(d->usb,ep);
            // Wait for cancellation ownership to return before freeing callback context.
            while(!done)ax_pump(10);
            if(!d->gone)usb_host_endpoint_clear(d->usb,ep);
            e=ESP_ERR_TIMEOUT;
        } else if(t->status!=USB_TRANSFER_STATUS_COMPLETED)e=ESP_FAIL;
        if(got)*got=t->actual_num_bytes;
        if(e==ESP_OK && (ep&0x80))memcpy(p,t->data_buffer,t->actual_num_bytes);
        if(e==ESP_OK && !(ep&0x80) && t->actual_num_bytes!=n)e=ESP_ERR_INVALID_SIZE;
    }
    usb_host_transfer_free(t);return e;
}
esp_err_t ax_data_tx(ax900_device_t *d,const uint8_t *ethernet,size_t length) {
    if(!d || d->gone || !d->associated || d->station==0xff || !d->data_ep)return ESP_ERR_INVALID_STATE;
    if(length<14 || length>1518)return ESP_ERR_INVALID_SIZE;
    size_t n=4+sizeof(struct aic_wire_tx_host_descriptor)+length-14;
    uint8_t *frame=calloc(1,n);if(!frame)return ESP_ERR_NO_MEM;
    put16(frame,n);frame[2]=AIC_USB_TYPE_DATA_TX;
    struct aic_wire_tx_host_descriptor *h=(void *)(frame+4);
    h->packet_length=length-14;memcpy(&h->destination,ethernet,6);memcpy(&h->source,ethernet+6,6);
    memcpy(&h->ethertype,ethernet+12,2);h->access_category=1;h->tid=d->qos?0:0xff;
    h->vif_index=d->vif;h->station_index=d->station;
    memcpy(frame+4+sizeof(*h),ethernet+14,length-14);
    esp_err_t e=transfer(d,d->data_ep,frame,n,NULL);free(frame);
    if(e==ESP_OK)ax_packet_count(true,false);
    return e;
}
esp_err_t ax_command(ax900_device_t *d,uint16_t req,uint16_t cfm,const void *p,size_t n,void *r,size_t cap,size_t *got) {
    if(d->gone || n>2048 || (n&&!p))return ESP_ERR_INVALID_STATE;
    if(!d->rx) {
        TRY(usb_host_transfer_alloc(16384,0,&d->rx));
        d->rx->device_handle=d->usb;d->rx->bEndpointAddress=d->in_ep;
        d->rx->num_bytes=16384;d->rx->callback=receive_cb;d->rx->context=d;
        TRY(usb_host_transfer_submit(d->rx));d->rx_pending=true;
    }
    uint8_t *frame=calloc(1,n+16);if(!frame)return ESP_ERR_NO_MEM;
    put16(frame,n+12);frame[2]=0x11;put16(frame+8,req);
    put16(frame+10,req>>10);put16(frame+12,100);put16(frame+14,n);
    if(n)memcpy(frame+16,p,n);
    d->waiting_id=cfm;d->reply_done=false;d->reply_len=0;d->reply_error=ESP_OK;
    esp_err_t e=transfer(d,d->out_ep,frame,n+16,NULL);free(frame);
    if(e==ESP_OK && cfm) {
        int64_t until=esp_timer_get_time()+5000000;
        while(!d->reply_done && !d->gone && esp_timer_get_time()<until)ax_pump(10);
        if(d->gone)e=ESP_ERR_INVALID_STATE;
        else if(!d->reply_done)e=ESP_ERR_TIMEOUT;
        else e=d->reply_error;
        if(e==ESP_OK) {
            if(got)*got=d->reply_len;
            if(r && d->reply_len>cap)e=ESP_ERR_INVALID_SIZE;
            else if(r)memcpy(r,d->reply,d->reply_len);
        }
    }
    d->waiting_id=0;
    if(e!=ESP_OK)ESP_LOGE(TAG,"command %04x -> %04x: %s",req,cfm,esp_err_to_name(e));
    return e;
}
static void event_cb(const usb_host_client_event_msg_t *e,void *arg) {
    if(e->event==USB_HOST_CLIENT_EVENT_NEW_DEV && e->new_dev.address<128)pending[e->new_dev.address]=true;
    if(e->event==USB_HOST_CLIENT_EVENT_DEV_GONE) {
        for(int i=1;i<128;i++)if(opened[i]==e->dev_gone.dev_hdl)removed[i]=true;
        if(active && active->usb==e->dev_gone.dev_hdl)active->gone=true;
    }
}
static void stop_device(ax900_device_t *d) {
    d->stopping=true;
    if(d->runtime)ax_net_stop(d);
    if(d->rx_pending) {
        usb_host_endpoint_halt(d->usb,d->in_ep);usb_host_endpoint_flush(d->usb,d->in_ep);
        while(d->rx_pending)ax_pump(10);
    }
    if(d->rx)usb_host_transfer_free(d->rx);
    usb_host_interface_release(client,d->usb,d->interface);
    free(d);active=NULL;
}
static esp_err_t attach_device(uint8_t address) {
    TRY(usb_host_device_open(client,address,&opened[address]));
    const usb_device_desc_t *desc;const usb_config_desc_t *cfg;
    TRY(usb_host_get_device_descriptor(opened[address],&desc));
    bool msc=desc->idProduct==0x5721,boot=desc->idProduct==0x8d80;
    bool runtime=desc->idProduct==0x8d81 || desc->idProduct==0x8d41;
    if(desc->idVendor!=0xa69c || !(msc||boot||runtime) || active) {
        usb_host_device_close(client,opened[address]);opened[address]=NULL;return ESP_OK;
    }
    ESP_LOGI(TAG,"Found %04x:%04x",desc->idVendor,desc->idProduct);
    TRY(usb_host_get_active_config_descriptor(opened[address],&cfg));
    usb_print_config_descriptor(cfg,NULL);
    ax900_device_t *d=calloc(1,sizeof(*d));if(!d)return ESP_ERR_NO_MEM;
    d->usb=opened[address];d->runtime=runtime;d->station=0xff;
    const uint8_t *raw=(const uint8_t *)cfg;bool selected=false;
    for(size_t off=cfg->bLength;off+2<=cfg->wTotalLength;) {
        size_t len=raw[off];if(len<2 || off+len>cfg->wTotalLength)break;
        if(raw[off+1]==4 && len>=9) {
            if(selected)break;
            selected=raw[off+3]==0 && raw[off+5]==(msc?8:0xff);
            if(selected)d->interface=raw[off+2];
        } else if(selected && raw[off+1]==5 && len>=7 && (raw[off+3]&3)==2) {
            // Runtime D80 adds command OUT 0x04 alongside the data endpoint.
            uint8_t ep=raw[off+2];
            if(runtime && ep==1)d->data_ep=ep;
            if((ep&0x80) && !d->in_ep)d->in_ep=ep;
            if(!(ep&0x80) && (!d->out_ep || (runtime && ep==4)))d->out_ep=ep;
        }
        off+=len;
    }
    if(!d->in_ep||!d->out_ep){free(d);return ESP_ERR_NOT_SUPPORTED;}
    esp_err_t e=usb_host_interface_claim(client,d->usb,d->interface,0);
    if(e!=ESP_OK){free(d);return e;}active=d;
    taskENTER_CRITICAL(&lock);state.present=true;taskEXIT_CRITICAL(&lock);
    if(msc) {
        ax_status("Switching AX900 to Wi-Fi mode");
        uint8_t cbw[31]={0x55,0x53,0x42,0x43,0x78,0x56,0x34,0x12};
        cbw[14]=6;cbw[15]=0x1b;cbw[19]=2;
        e=transfer(d,d->out_ep,cbw,sizeof(cbw),NULL);
        if(e==ESP_OK){uint8_t csw[512];size_t got=0;e=transfer(d,d->in_ep,csw,sizeof(csw),&got);if(e==ESP_OK && (got!=13 || memcmp(csw,"USBS",4) || csw[12]))e=ESP_FAIL;}
        stop_device(d);
    } else if(boot) {
        ax_status("Loading AX900 wireless firmware");
        e=ax_load_firmware(d);
        stop_device(d);
        if(e==ESP_OK)ax_status("Waiting for AX900 wireless firmware");
    } else {
        ax_status("Initializing AX900 radio");
        e=ax_runtime_init(d);
        if(e==ESP_OK)e=ax_net_init(d);
        if(e==ESP_OK)ax900_request_scan();
    }
    return e;
}
static void client_task(void *arg) {
    for(;;) {
        ax_pump(20);
        taskENTER_CRITICAL(&lock);bool forget=forget_requested;forget_requested=false;
        if(forget && requested_connection)requested_connection->skip_save=true;
        taskEXIT_CRITICAL(&lock);
        if(forget){ax_net_disable_save();esp_err_t e=ax_profile_forget();ax_profile_status(false,e);}
        for(int i=1;i<128;i++) {
            if(removed[i]) {
                removed[i]=false;
                if(active && active->usb==opened[i]) {
                    stop_device(active);
                    taskENTER_CRITICAL(&lock);state.ready=false;state.present=false;state.scanning=false;state.ap_count=0;scan_requested=false;connect_requested=false;disconnect_requested=false;ax_connect_request_t *cancelled=requested_connection;requested_connection=NULL;taskEXIT_CRITICAL(&lock);
                    ax_free_connect_request(cancelled);
                    ax_status("AX900 disconnected");
                }
                if(opened[i])usb_host_device_close(client,opened[i]);
                opened[i]=NULL;
            }
            if(pending[i]) {
                pending[i]=false;
                esp_err_t e=attach_device(i);
                if(e!=ESP_OK){char msg[96];snprintf(msg,sizeof(msg),"AX900 initialization failed: %s",esp_err_to_name(e));ax_status(msg);}
            }
        }
        if(active && active->runtime && !active->gone) {
            taskENTER_CRITICAL(&lock);
            bool disconnect=disconnect_requested;disconnect_requested=false;
            bool connect=connect_requested;connect_requested=false;
            ax_connect_request_t *request=requested_connection;requested_connection=NULL;
            taskEXIT_CRITICAL(&lock);
            if(disconnect){ax_net_disconnect(active,3);connect=false;ax_free_connect_request(request);request=NULL;}
            if(connect){
                esp_err_t e=ax_net_connect(active,request);
                if(e!=ESP_OK){ax_link_state(false,false,false,NULL,0);ax_status("Connection request failed");}
            }
            ax_net_poll(active);
        }
        taskENTER_CRITICAL(&lock);
        bool scan=scan_requested && state.ready && !state.scanning && !state.connecting && !state.associated;
        if(scan){scan_requested=false;state.scanning=true;state.ap_count=0;}
        taskEXIT_CRITICAL(&lock);
        if(scan && active && !active->gone) {
            ax_status("Scanning 2.4 / 5 GHz");
            esp_err_t e=ax_scan(active);
            taskENTER_CRITICAL(&lock);state.scanning=false;state.scan_generation++;size_t count=state.ap_count;taskEXIT_CRITICAL(&lock);
            char msg[96];snprintf(msg,sizeof(msg),"%s: %u networks",e==ESP_OK?"Scan complete":"Scan failed",(unsigned)count);ax_status(msg);
            taskENTER_CRITICAL(&lock);bool reconnect=auto_reconnect_pending && !connect_requested && !disconnect_requested;taskEXIT_CRITICAL(&lock);
            if(e==ESP_OK && reconnect) {
                ax900_status_t *snapshot=malloc(sizeof(*snapshot));
                if(snapshot){ax900_get_status(snapshot);ax_connect_request_t *r=ax_profile_find(snapshot->aps,snapshot->ap_count);free(snapshot);
                    if(r){ESP_LOGI(TAG,"Reconnecting using saved Wi-Fi profile");(void)queue_connect(r);}}
            }
        }
    }
}
esp_err_t ax900_start(void) {
    if(client)return ESP_ERR_INVALID_STATE;
    esp_err_t profile_error=ax_profile_init();ax_profile_status(false,profile_error);
    usb_host_client_config_t cfg={.is_synchronous=false,.max_num_event_msg=10,.async={.client_event_callback=event_cb}};
    TRY(usb_host_client_register(&cfg,&client));
    ax_status("Waiting for AX900");
    if(xTaskCreate(client_task,"ax900",24576,NULL,5,NULL)!=pdPASS) {usb_host_client_deregister(client);client=NULL;return ESP_ERR_NO_MEM;}
    return ESP_OK;
}
