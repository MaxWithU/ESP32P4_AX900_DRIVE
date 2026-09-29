// SPDX-License-Identifier: Apache-2.0
// Compile the actual worker/transport with a USB host that enforces transfer ownership.
#include <assert.h>
#include <stdio.h>
#include "../components/ax900/ax900_usb.c"

static int64_t now;
static usb_transfer_t *owned[8];
static unsigned allocations, closes, net_stops, net_inits, scans, connections;
static bool finish_tx=true, finish_cancel=true, fail_submit, fail_descriptor, fail_init, fail_net_init, cancel_during_scan;
static unsigned saved_count=1;
static ax900_ap_t synthetic_ap;
static ax900_ap_t alternate_ap;
static unsigned last_candidates;
static bool connected_skip_save;
int64_t esp_timer_get_time(void){return now;}
static void complete(usb_transfer_t *t,int status){
    bool found=false;
    for(unsigned i=0;i<8;i++)if(owned[i]==t){owned[i]=NULL;found=true;break;}
    assert(found);t->status=status;t->actual_num_bytes=status==0?t->num_bytes:0;t->callback(t);
}
esp_err_t usb_host_transfer_alloc(size_t n,int iso,usb_transfer_t **out){
    (void)iso;*out=calloc(1,sizeof(**out));assert(*out);(*out)->data_buffer=calloc(1,n);allocations++;return ESP_OK;
}
esp_err_t usb_host_transfer_free(usb_transfer_t *t){
    for(unsigned i=0;i<8;i++)assert(owned[i]!=t);
    free(t->data_buffer);free(t);allocations--;return ESP_OK;
}
esp_err_t usb_host_transfer_submit(usb_transfer_t *t){
    if(fail_submit)return ESP_FAIL;
    for(unsigned i=0;i<8;i++)assert(owned[i]!=t);
    for(unsigned i=0;i<8;i++)if(!owned[i]){owned[i]=t;return ESP_OK;}
    assert(0);return ESP_FAIL;
}
esp_err_t usb_host_transfer_submit_control(usb_host_client_handle_t c,usb_transfer_t *t){(void)c;assert(t->num_bytes==8 && t->data_buffer[0]==2 && t->data_buffer[1]==1);return usb_host_transfer_submit(t);}
esp_err_t usb_host_endpoint_halt(usb_device_handle_t d,uint8_t ep){(void)d;(void)ep;return ESP_OK;}
esp_err_t usb_host_endpoint_clear(usb_device_handle_t d,uint8_t ep){(void)d;(void)ep;return ESP_OK;}
esp_err_t usb_host_endpoint_flush(usb_device_handle_t d,uint8_t ep){
    if(finish_cancel)for(unsigned i=0;i<8;i++)if(owned[i] && owned[i]->device_handle==d && owned[i]->bEndpointAddress==ep)complete(owned[i],USB_TRANSFER_STATUS_CANCELED);
    return ESP_OK;
}
esp_err_t usb_host_client_handle_events(usb_host_client_handle_t c,unsigned ticks){
    (void)c;now+=(int64_t)ticks*1000;
    if(finish_tx)for(unsigned i=0;i<8;i++)if(owned[i] && !(owned[i]->bEndpointAddress&0x80))complete(owned[i],USB_TRANSFER_STATUS_COMPLETED);
    return ESP_OK;
}
esp_err_t usb_host_device_open(usb_host_client_handle_t c,uint8_t address,usb_device_handle_t *d){(void)c;*d=(void *)(uintptr_t)address;return ESP_OK;}
esp_err_t usb_host_device_close(usb_host_client_handle_t c,usb_device_handle_t d){
    (void)c;for(unsigned i=0;i<8;i++)assert(!owned[i] || owned[i]->device_handle!=d);closes++;return ESP_OK;
}
esp_err_t usb_host_get_device_descriptor(usb_device_handle_t d,const usb_device_desc_t **desc){
    (void)d;static usb_device_desc_t v={0xa69c,0x8d81};*desc=&v;return fail_descriptor?ESP_FAIL:ESP_OK;
}
esp_err_t usb_host_get_active_config_descriptor(usb_device_handle_t d,const usb_config_desc_t **cfg){
    (void)d;static const uint8_t raw[]={9,2,39,0,0,0,0,0,0,9,4,0,0,3,0xff,0,0,0,7,5,0x81,2,0,2,0,7,5,4,2,0,2,0,7,5,1,2,0,2,0};
    *cfg=(const void *)raw;return ESP_OK;
}
esp_err_t usb_host_interface_claim(usb_host_client_handle_t c,usb_device_handle_t d,uint8_t i,uint8_t a){(void)c;(void)d;(void)i;(void)a;return ESP_OK;}
esp_err_t usb_host_interface_release(usb_host_client_handle_t c,usb_device_handle_t d,uint8_t i){(void)c;(void)d;(void)i;return ESP_OK;}
esp_err_t usb_host_client_register(const usb_host_client_config_t *cfg,usb_host_client_handle_t *c){(void)cfg;*c=(void *)1;return ESP_OK;}
esp_err_t usb_host_device_addr_list_fill(int size,uint8_t *addresses,int *count){assert(size>=1);addresses[0]=1;*count=1;return ESP_OK;}
esp_err_t usb_host_client_deregister(usb_host_client_handle_t c){(void)c;return ESP_OK;}
void usb_print_config_descriptor(const usb_config_desc_t *cfg,void *p){(void)cfg;(void)p;}
void ax_parse_security(ax900_ap_t *ap){(void)ap;}
esp_err_t ax_load_firmware(ax900_device_t *d){(void)d;return ESP_OK;}
esp_err_t ax_runtime_init(ax900_device_t *d){d->supports_5ghz=true;return fail_init?ESP_FAIL:ESP_OK;}
esp_err_t ax_net_init(ax900_device_t *d){(void)d;net_inits++;return fail_net_init?ESP_FAIL:ESP_OK;}
void ax_net_stop(ax900_device_t *d){(void)d;net_stops++;ax_link_state(false,false,false,NULL,0);}
void ax_net_poll(ax900_device_t *d){(void)d;}
void ax_net_receive(void *arg,const uint8_t *p,size_t n){(void)arg;(void)p;(void)n;}
void ax_net_disable_save(void){}
void ax_net_debug_dhcp_timeout(void){}
void ax_net_disconnect(ax900_device_t *d,uint16_t reason){(void)d;ax_link_state(false,false,false,NULL,reason);}
esp_err_t ax_net_connect(ax900_device_t *d,ax_connect_request_t *r){(void)d;connections++;connected_skip_save=r->skip_save;ax_link_state(false,true,true,NULL,0);ax_ip_state("192.0.2.1");ax_free_connect_request(r);return ESP_OK;}
bool ax_profile_auto_enabled(const ax900_ap_t *ap){(void)ap;return true;}
esp_err_t ax_profile_forget_network(const ax900_ap_t *ap){(void)ap;return ESP_OK;}
esp_err_t ax_profile_set_auto(const ax900_ap_t *ap,bool enabled){(void)ap;(void)enabled;return ESP_OK;}
esp_err_t ax_net_deinit(void){return ESP_OK;}
void ax900_test_cancel(void){}
void ax900_test_get_result(ax900_test_result_t *r){memset(r,0,sizeof(*r));}
void ax900_probe_get_result(ax900_probe_result_t *r){memset(r,0,sizeof(*r));}
esp_err_t ax_profile_init(void){return ESP_OK;}
unsigned ax_profile_count(void){return saved_count;}
unsigned ax_profile_auto_count(void){return saved_count;}
esp_err_t ax_profile_forget(void){saved_count=0;return ESP_OK;}
ax_connect_request_t *ax_profile_find(const ax900_ap_t *aps,size_t count){
    last_candidates=count;
    if(!saved_count || !count)return NULL;
    ax_connect_request_t *r=calloc(1,sizeof(*r));r->ap=aps[0];return r;
}
ax_connect_request_t *ax_profile_find_auto(const ax900_ap_t *aps,size_t count){return ax_profile_find(aps,count);}
esp_err_t ax_scan(ax900_device_t *d){
    (void)d;scans++;state.ap_count=2;state.aps[0]=synthetic_ap;state.aps[1]=alternate_ap;
    if(cancel_during_scan)assert(ax900_disconnect()==ESP_OK);
    return ESP_OK;
}
static void reset(void){
    assert(!allocations && !active && !requested_connection);
    memset(&state,0,sizeof(state));memset(pending,0,sizeof(pending));memset(removed,0,sizeof(removed));memset(opened,0,sizeof(opened));
    memset(usb_retry_at,0,sizeof(usb_retry_at));memset(usb_attempts,0,sizeof(usb_attempts));
    recovery=(ax_recovery_t){.enabled=true,.pending=true};target_valid=false;now=0;
    finish_tx=finish_cancel=true;fail_submit=fail_descriptor=fail_init=fail_net_init=cancel_during_scan=false;
    scan_requested=connect_requested=disconnect_requested=forget_requested=false;client=(void *)1;
    lifecycle=AX900_RUNNING;operation_active=false;profile_action=0;resume_requested=false;
    saved_count=1;closes=net_stops=net_inits=scans=connections=last_candidates=0;
    synthetic_ap=(ax900_ap_t){.ssid_len=4,.raw_ssid="test",.frequency=5260};
    alternate_ap=(ax900_ap_t){.ssid_len=5,.raw_ssid="other",.frequency=5180};
}
static ax900_device_t *device(void){
    ax900_device_t *d=calloc(1,sizeof(*d));d->usb=(void *)1;d->address=1;d->in_ep=0x81;d->out_ep=4;d->runtime=true;
    active=d;opened[1]=d->usb;state.ready=true;state.present=true;return d;
}
static void add_rx(ax900_device_t *d){
    assert(usb_host_transfer_alloc(16384,0,&d->rx)==ESP_OK);d->rx->device_handle=d->usb;d->rx->bEndpointAddress=d->in_ep;
    d->rx->context=d;d->rx->callback=receive_cb;d->rx_pending=true;assert(usb_host_transfer_submit(d->rx)==ESP_OK);
}
static void policy_test(void){
    ax_recovery_t r={0};ax_recovery_select(&r,true);ax_recovery_lost(&r,0);
    const int64_t delays[]={1000000,2000000,4000000,8000000,16000000};
    int64_t clock=0;
    for(unsigned i=0;i<5;i++){
        assert(!ax_recovery_take(&r,clock+delays[i]-1));clock+=delays[i];assert(ax_recovery_take(&r,clock));
        ax_recovery_online(&r,clock);ax_recovery_lost(&r,clock); // immediate flap must not reset budget
    }
    assert(!r.enabled && !r.pending && r.attempts==5);
    ax_recovery_select(&r,true);ax_recovery_lost(&r,clock);assert(ax_recovery_take(&r,clock+1000000));
    ax_recovery_online(&r,clock+1000000);ax_recovery_online(&r,clock+61000000);assert(r.attempts==0);
    ax_recovery_select(&r,false);ax_recovery_lost(&r,clock+62000000);assert(!r.pending);
}
int main(void){
    policy_test();reset();
    // Late callback after both call timeout and teardown timeout: still owns heap objects.
    ax900_device_t *d=device();add_rx(d);finish_tx=finish_cancel=false;
    uint8_t bytes[16]={0};assert(transfer(d,4,bytes,sizeof(bytes),NULL)==ESP_ERR_TIMEOUT);
    assert(now==3500000 && d->tx && d->fault && !state.ready);
    usb_transfer_t *late=d->tx->transfer;assert(!stop_device(d));assert(allocations==2 && closes==0);
    now+=1000000;assert(!stop_device(d));assert(d->drain_reported);
    complete(late,USB_TRANSFER_STATUS_CANCELED);assert(!stop_device(d));
    complete(d->rx,USB_TRANSFER_STATUS_CANCELED);assert(stop_device(d));assert(allocations==0 && closes==1 && net_stops==1);
    // RX stall and re-submit failure both invalidate the public connected state.
    reset();d=device();add_rx(d);state.has_ip=true;
    complete(d->rx,USB_TRANSFER_STATUS_STALL);assert(d->fault && !state.ready && !state.has_ip);assert(stop_device(d));
    reset();d=device();add_rx(d);fail_submit=true;
    complete(d->rx,USB_TRANSFER_STATUS_COMPLETED);assert(d->fault && !d->rx_pending);assert(stop_device(d));
    // Initial RX submission failure is cleaned up too.
    reset();d=device();fail_submit=true;
    assert(ax_command(d,1,2,NULL,0,NULL,0,NULL)!=ESP_OK);assert(stop_device(d));assert(!allocations);
    // Early descriptor failures close their handle; radio failures never advertise ready.
    reset();fail_descriptor=true;attach_device(1);assert(closes==1 && !opened[1] && !active);
    reset();fail_init=true;pending[1]=true;
    for(unsigned i=0;i<12;i++){client_step();now+=10000000;}
    assert(active && active->stopping && !state.ready && !usb_retry_at[1] && state.usb_recoveries==3 && closes==0 && net_inits==0);assert(stop_device(active));
    reset();fail_net_init=true;attach_device(1);assert(!state.ready && active && active->fault);assert(stop_device(active));
    // Command-reply timeout poisons this transport so a late reply cannot match a later request.
    reset();d=device();assert(ax_command(d,0x100,0x101,NULL,0,NULL,0,NULL)==ESP_ERR_TIMEOUT);
    assert(d->fault && !state.ready && !d->tx);assert(stop_device(d));assert(!allocations);
    // Successful TX plus cancellation completion returns every transfer once.
    reset();d=device();assert(transfer(d,4,bytes,sizeof(bytes),NULL)==ESP_OK);assert(!allocations && !d->tx);assert(stop_device(d));
    reset();d=device();finish_tx=false;assert(transfer(d,4,bytes,sizeof(bytes),NULL)==ESP_ERR_TIMEOUT);
    assert(!d->tx && !allocations);assert(stop_device(d));
    // Actual worker reconnects after a transport fault and retains the selected SSID.
    reset();d=device();add_rx(d);target=synthetic_ap;target_valid=true;recovery.pending=false;
    complete(d->rx,USB_TRANSFER_STATUS_STALL);client_step();assert(active && active->stopping && recovery.pending);
    now+=2000000;client_step();assert(active && state.ready && connect_requested && last_candidates==1);
    client_step();assert(connections==1 && state.has_ip);assert(stop_device(active));
    // DEV_GONE waits for a late RX completion; the reattached adapter uses the saved target.
    reset();d=device();add_rx(d);finish_cancel=false;recovery.pending=false;target=synthetic_ap;target_valid=true;
    usb_host_client_event_msg_t gone={.event=USB_HOST_CLIENT_EVENT_DEV_GONE,.dev_gone={.dev_hdl=d->usb}};
    event_cb(&gone,NULL);client_step();assert(active && active->stopping && recovery.pending && closes==0);
    complete(d->rx,USB_TRANSFER_STATUS_CANCELED);client_step();assert(!active && closes==1 && !state.present);
    finish_cancel=true;now+=2000000;usb_host_client_event_msg_t added={.event=USB_HOST_CLIENT_EVENT_NEW_DEV,.new_dev={.address=2}};
    event_cb(&added,NULL);client_step();client_step();assert(connections==1 && state.has_ip && last_candidates==1);assert(stop_device(active));
    // Authentication rejection pauses even if hardware is reattached.
    reset();ax_reconnect_lost(true);pending[1]=true;client_step();client_step();assert(connections==0 && !recovery.enabled);assert(stop_device(active));
    // User disconnect while scan is in progress invalidates its queued automatic result.
    reset();d=device();cancel_during_scan=true;client_step();assert(!connect_requested && !recovery.enabled);client_step();assert(connections==0);assert(stop_device(d));
    // Manual disconnect survives unplug/replug, including requests while USB is absent.
    reset();assert(ax900_disconnect()==ESP_OK);pending[1]=true;client_step();client_step();assert(!recovery.enabled && !connections);assert(stop_device(active));
    // Forget cancels an already queued saved-profile attempt.
    reset();d=device();client_step();assert(connect_requested);assert(ax900_forget_saved()==ESP_OK);client_step();assert(!connections && !saved_count && !requested_connection);assert(stop_device(d));
    // Turning off auto-connect does not discard a user's pending password update.
    reset();d=device();recovery.pending=false;
    ax_connect_request_t *manual=calloc(1,sizeof(*manual));manual->ap=synthetic_ap;
    assert(queue_connect(manual)==ESP_OK);
    assert(ax900_set_auto_connect(&synthetic_ap,false)==ESP_OK);client_step();
    assert(connections==1 && !connected_skip_save && !recovery.enabled);assert(stop_device(d));
    // Async slots stay USB-owned through a removal timeout and late completion.
    reset();d=device();d->data_ep=1;d->associated=true;d->station=0;
    uint8_t ethernet[100]={0};finish_tx=false;finish_cancel=false;
    for(unsigned i=0;i<AX_DATA_SLOTS;i++)assert(ax_data_tx_async(d,ethernet,sizeof(ethernet))==ESP_OK);
    assert(!ax_data_tx_available(d) && allocations==AX_DATA_SLOTS);
    assert(ax_data_tx_async(d,ethernet,sizeof(ethernet))==ESP_ERR_NO_MEM);
    d->gone=true;assert(!stop_device(d));assert(closes==0);
    for(unsigned i=0;i<AX_DATA_SLOTS;i++)complete(d->data[i].transfer,USB_TRANSFER_STATUS_CANCELED);
    assert(stop_device(d));assert(!allocations);
    // Completed slots reuse their DMA allocation; incomplete transfers trip watchdog.
    reset();d=device();d->data_ep=1;d->associated=true;d->station=0;
    assert(ax_data_tx_async(d,ethernet,sizeof(ethernet))==ESP_OK);ax_pump(1);
    assert(ax_data_tx_available(d) && allocations==1 && state.tx_packets==1);
    assert(ax_data_tx_async(d,ethernet,sizeof(ethernet))==ESP_OK);finish_tx=false;now+=3000001;data_watchdog(d);
    assert(d->fault);assert(stop_device(d));
    reset();d=device();add_rx(d);finish_cancel=false;
    assert(ax900_stop()==ESP_OK && ax900_get_lifecycle()==AX900_STOPPING);
    assert(ax900_start()==ESP_ERR_INVALID_STATE);client_step();assert(active && client);
    complete(d->rx,USB_TRANSFER_STATUS_CANCELED);client_step();
    assert(ax900_get_lifecycle()==AX900_STOPPED && active==d && client && !allocations);
    assert(ax900_start()==ESP_OK);client_step();assert(state.ready && active==d);client_step();
    assert(ax900_stop()==ESP_OK);client_step();assert(ax900_get_lifecycle()==AX900_STOPPED);
    // USB removal is still observed while stopped; never reuse state by address.
    gone.dev_gone.dev_hdl=d->usb;event_cb(&gone,NULL);client_step();assert(!active && !state.present);
    added.new_dev.address=2;event_cb(&added,NULL);client_step();assert(!active && pending[2]);
    assert(ax900_start()==ESP_OK);client_step();assert(active && active->address==2);
    assert(stop_device(active));client=NULL;lifecycle=AX900_STOPPED;
    // A newly registered client must discover already enumerated devices, and
    // must not take the event loop before official Wi-Fi initializes.
    assert(ax900_start()==ESP_OK && pending[1]);
    assert(esp_event_loop_create_default()==ESP_OK);
    assert(ax900_stop()==ESP_OK);client_step();assert(ax900_get_lifecycle()==AX900_STOPPED);
    assert(!allocations);puts("Recovery tests passed: production USB ownership, late callbacks, initialization cleanup, retry budget, target selection, cancellation and auth pause");
}
