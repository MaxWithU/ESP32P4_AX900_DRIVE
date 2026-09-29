// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "../components/ax900/ax900_wifi.c"
static bool firmware_running,fail_power;
static unsigned starts,calibrations,resets,interfaces;
esp_err_t ax_command(ax900_device_t *d,uint16_t req,uint16_t cfm,const void *p,size_t n,void *r,size_t cap,size_t *got){
    (void)d;(void)cfm;(void)p;(void)n;if(r)memset(r,0,cap);if(got)*got=0;
    switch(req){
    case AIC_MM_SET_STACK_START_REQ:
        assert(!starts++);assert(cap==sizeof(struct aic_wire_mm_set_stack_start_cfm));
        ((struct aic_wire_mm_set_stack_start_cfm *)r)->supports_5ghz=1;*got=cap;break;
    case AIC_MM_SET_TXPWR_REQ:
        assert(!firmware_running);if(fail_power){fail_power=false;return ESP_FAIL;}break;
    case AIC_MM_SET_RF_CALIB_REQ:assert(!firmware_running);calibrations++;break;
    case AIC_MM_GET_MAC_REQ:
        assert(!firmware_running);assert(cap==6);memcpy(r,"\x02\x01\x02\x03\x04\x05",6);*got=6;break;
    case AIC_MM_RESET_REQ:resets++;break;
    case AIC_MM_VERSION_REQ:*got=cap;break;
    case AIC_MM_ADD_IF_REQ:assert(cap==2);((uint8_t *)r)[1]=interfaces++;*got=2;firmware_running=true;break;
    default:break;
    }
    return ESP_OK;
}
void ax_status(const char *s){(void)s;}
void ax900_get_status(ax900_status_t *s){memset(s,0,sizeof(*s));}
void ax_pump(unsigned ms){(void)ms;}
int64_t esp_timer_get_time(void){return 0;}
void ax900_get_radio_config(ax900_radio_config_t *out){*out=(ax900_radio_config_t){.country="CN",.allow_dfs=true};}
void ax_radio_report(uint32_t version,uint32_t features){(void)version;(void)features;}
int main(void){
    ax900_device_t d={0};fail_power=true;
    assert(ax_runtime_init(&d)==ESP_FAIL);assert(d.radio_started && !d.radio_configured);
    assert(ax_runtime_init(&d)==ESP_OK);assert(starts==1 && calibrations==1 && d.supports_5ghz);
    uint8_t original_mac[6];memcpy(original_mac,d.mac,6);
    for(unsigned i=0;i<4;i++)assert(ax_runtime_init(&d)==ESP_OK);
    assert(starts==1 && calibrations==1 && resets==5 && interfaces==5);
    assert(!memcmp(original_mac,d.mac,6));
    puts("Radio recovery passed: partial cold initialization resumes; warm reset preserves MAC/band and skips stack start/RF calibration");
}
