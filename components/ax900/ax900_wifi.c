/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd
 * SPDX-License-Identifier: Apache-2.0
 * D80 radio/scan port from RT-Smart aic8800_radio.c and aic8800_wifi.c.
 */
#include "ax900_internal.h"
#include "aic8800_protocol.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include <stdlib.h>

// CN (SRRC) channels from the matching vendor powerlimit file. Passive scans.
static const uint8_t channels5[]={36,40,44,48,52,56,60,64,149,153,157,161,165};
static void channel(uint8_t *out,unsigned number,bool band5,bool passive) {
    put16(out,band5?5000+5*number:2407+5*number);
    out[2]=band5;out[3]=passive?1:0; // Passive applies to scans, not the operating channel table.
    out[4]=band5?(number<149?15:20):(number==13?13:number==12?15:16);
}
static esp_err_t command(ax900_device_t *d,uint16_t req,uint16_t cfm,const void *p,size_t n) {
    return ax_command(d,req,cfm,p,n,NULL,0,NULL);
}
esp_err_t ax_runtime_init(ax900_device_t *d) {
    struct aic_wire_mm_set_stack_start_req stack={.start=1,.vendor_info=0x20};
    struct aic_wire_mm_set_stack_start_cfm stack_reply;size_t got=0;
    TRY(ax_command(d,AIC_MM_SET_STACK_START_REQ,AIC_MM_SET_STACK_START_CFM,&stack,sizeof(stack),&stack_reply,sizeof(stack_reply),&got));
    if(got!=sizeof(stack_reply))return ESP_ERR_INVALID_RESPONSE;
    ESP_LOGI("AX900","STACK_STARTED supports_5ghz=%u vendor=%02x",stack_reply.supports_5ghz,stack_reply.vendor_info);
    // Exact values from usb/aic8800D80/aic_userconfig_8800d80.txt.
    struct aic_wire_mm_set_tx_power_req power={.configuration.v3={
        .enable=1,
        .legacy_2ghz={18,18,18,18,18,18,18,18,16,16,15,15},
        .ht_vht_2ghz={18,18,18,18,16,16,15,15,14,14},
        .he_2ghz={18,18,18,18,16,16,15,15,14,14,13,13},
        .legacy_5ghz={-128,-128,-128,-128,18,18,18,18,16,16,15,15},
        .ht_vht_5ghz={18,18,18,18,16,16,15,15,14,14},
        .he_5ghz={18,18,18,18,16,16,14,14,13,13,12,12}}};
    TRY(command(d,AIC_MM_SET_TXPWR_REQ,AIC_MM_SET_TXPWR_CFM,&power,sizeof(power)));
    struct aic_wire_mm_set_rf_calibration_req rf={.calibration_2ghz=0x0f8f,.calibration_5ghz=0x0f0f,.alpha=0x0c34c008,.bluetooth_parameter=0x00264203};
    TRY(command(d,AIC_MM_SET_RF_CALIB_REQ,AIC_MM_SET_RF_CALIB_CFM,&rf,sizeof(rf)));
    uint32_t getmac=1;
    TRY(ax_command(d,AIC_MM_GET_MAC_REQ,AIC_MM_GET_MAC_CFM,&getmac,4,d->mac,6,&got));
    if(got!=6)return ESP_ERR_INVALID_RESPONSE;
    if((d->mac[0]&1) || !memcmp(d->mac,"\0\0\0\0\0\0",6)) {
        // Some adapters have no MAC in efuse. Vendor Linux also falls back.
        // Derive a stable unicast address for this board without programming efuses.
        uint8_t base[6];TRY(esp_read_mac(base,ESP_MAC_EFUSE_FACTORY));
        TRY(esp_derive_local_mac(d->mac,base));d->mac[5]^=0xa9;
        ESP_LOGW("AX900","Adapter returned an invalid MAC; using stable board-derived local address");
    }
    ESP_LOGI("AX900","Station MAC %02x:%02x:%02x:%02x:%02x:%02x",d->mac[0],d->mac[1],d->mac[2],d->mac[3],d->mac[4],d->mac[5]);
    TRY(command(d,AIC_MM_RESET_REQ,AIC_MM_RESET_CFM,NULL,0));
    struct aic_wire_mm_version_cfm version;
    TRY(ax_command(d,AIC_MM_VERSION_REQ,AIC_MM_VERSION_CFM,NULL,0,&version,sizeof(version),&got));
    if(got!=sizeof(version))return ESP_ERR_INVALID_RESPONSE;
    ESP_LOGI("AX900","RUNTIME_VERSION=%08lx features=%08lx PHY=%08lx",(unsigned long)version.lmac_version,(unsigned long)version.features,(unsigned long)version.phy_version1);
    // Start conservatively at HT40; D40 variants can share the D80 USB identity.
    struct aic_wire_me_config_req me={0};
    me.ht.capability=0x0963;me.ht.ampdu_parameters=31;me.ht.mcs_rate[0]=0xff;me.ht.mcs_rate[4]=1;
    put16(me.ht.mcs_rate+10,150);me.ht.mcs_rate[12]=1;
    me.tx_lifetime=1000;me.ht_supported=1;me.max_bandwidth=1;
    TRY(command(d,AIC_ME_CONFIG_REQ,AIC_ME_CONFIG_CFM,&me,sizeof(me)));
    uint8_t channels[254]={0};
    for(unsigned i=0;i<13;i++)channel(channels+6*i,i+1,false,false);
    channels[252]=13;
    if(stack_reply.supports_5ghz) {
        for(unsigned i=0;i<sizeof(channels5);i++)channel(channels+84+6*i,channels5[i],true,false);
        channels[253]=sizeof(channels5);
    }
    TRY(command(d,AIC_ME_CHAN_CONFIG_REQ,AIC_ME_CHAN_CONFIG_CFM,channels,sizeof(channels)));
    struct aic_wire_mm_start_req start={.uapsd_timeout=300,.lp_clock_accuracy=20};
    TRY(command(d,AIC_MM_START_REQ,AIC_MM_START_CFM,&start,sizeof(start)));
    struct aic_wire_mm_set_coex_req coex={.bt_on=1,.enable_nullcts=1};
    TRY(command(d,AIC_MM_SET_COEX_REQ,AIC_MM_SET_COEX_CFM,&coex,sizeof(coex)));
    uint8_t add[10]={0},reply[2];memcpy(add+2,d->mac,6);
    TRY(ax_command(d,AIC_MM_ADD_IF_REQ,AIC_MM_ADD_IF_CFM,add,sizeof(add),reply,2,&got));
    if(got!=2 || reply[0])return ESP_ERR_INVALID_RESPONSE;
    d->vif=reply[1];ax_set_ready(stack_reply.supports_5ghz!=0);
    ax_status("AX900 radio ready");return ESP_OK;
}
esp_err_t ax_scan(ax900_device_t *d) {
    ax900_status_t *status=malloc(sizeof(*status));if(!status)return ESP_ERR_NO_MEM;
    ax900_get_status(status);bool band5=status->supports_5ghz;free(status);
    for(unsigned band=0;band<(band5?2:1);band++) {
        struct aic_wire_scanu_start_req scan={0};
        memset(&scan.bssid,0xff,sizeof(scan.bssid));scan.vif_index=d->vif;
        scan.channel_count=band?sizeof(channels5):13;
        for(unsigned i=0;i<scan.channel_count;i++)channel((uint8_t *)&scan.channels[i],band?channels5[i]:i+1,band,true);
        d->scan_done=false;d->scan_result=0xff;
        // Acceptance is only an acknowledgement; status arrives in SCANU_START_CFM.
        TRY(ax_command(d,AIC_SCANU_START_REQ,AIC_SCANU_START_ACCEPTED,&scan,sizeof(scan),NULL,0,NULL));
        int64_t until=esp_timer_get_time()+20000000;
        while(!d->scan_done && !d->gone && esp_timer_get_time()<until)ax_pump(20);
        if(!d->scan_done) {
            command(d,AIC_SCANU_CANCEL_REQ,AIC_SCANU_CANCEL_CFM,NULL,0);
            return ESP_ERR_TIMEOUT;
        }
        if(d->scan_result)return ESP_FAIL;
        // Allow trailing result records to drain before switching bands.
        for(int i=0;i<5;i++)ax_pump(20);
    }
    return ESP_OK;
}
