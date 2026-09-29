/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd
 * SPDX-License-Identifier: Apache-2.0
 * D80 loader adapted from RT-Smart aic8800_firmware.c for ESP-IDF.
 */
#include "ax900_internal.h"
#include <stdlib.h>
#include "esp_rom_sys.h"

#define BLOB(name) extern const uint8_t name##_start[] asm("_binary_" #name "_bin_start"); \
                   extern const uint8_t name##_end[] asm("_binary_" #name "_bin_end")
BLOB(fw_adid_8800d80_u02);
BLOB(fw_patch_8800d80_u02);
BLOB(fw_patch_table_8800d80_u02);
BLOB(fmacfw_8800d80_u02);

esp_err_t ax_mem_read(ax900_device_t *d,uint32_t address,uint32_t *value) {
    uint8_t req[4],reply[8];size_t n=0;put32(req,address);
    TRY(ax_command(d,0x400,0x401,req,4,reply,8,&n));
    if(n!=8 || get32(reply)!=address)return ESP_ERR_INVALID_RESPONSE;
    *value=get32(reply+4);return ESP_OK;
}
esp_err_t ax_mem_write(ax900_device_t *d,uint32_t address,uint32_t value) {
    uint8_t req[8],reply[8];size_t got=0;put32(req,address);put32(req+4,value);
    TRY(ax_command(d,0x402,0x403,req,8,reply,sizeof(reply),&got));
    // MMIO includes write-one-to-set/clear registers; only RAM must echo the value.
    if(got!=8 || get32(reply)!=address || (address<0x40000000 && get32(reply+4)!=value)) {
        ESP_LOGE("AX900","MEM_WRITE %08lx=%08lx reply len=%u",(unsigned long)address,(unsigned long)value,(unsigned)got);
        ESP_LOG_BUFFER_HEX("AX900",reply,got);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
static esp_err_t upload(ax900_device_t *d,const char *name,uint32_t address,const uint8_t *start,const uint8_t *end) {
    size_t length=end-start;
    if(length<4 || length>0x100000)return ESP_ERR_INVALID_SIZE;
    for(size_t off=0;off<length;off+=1024) {
        uint8_t req[1032]={0},reply[4];size_t n=length-off;if(n>1024)n=1024;
        put32(req,address+off);put32(req+4,(n+3)&~3U);memcpy(req+8,start+off,n);
        size_t got=0;TRY(ax_command(d,0x40b,0x40c,req,sizeof(req),reply,4,&got));
        if(got!=4 || get32(reply)!=0) {ESP_LOGE("AX900","Block %s offset=%u n=%u status=%08lx",name,(unsigned)off,(unsigned)n,(unsigned long)get32(reply));return ESP_ERR_INVALID_RESPONSE;}
    }
    size_t last_off=(length-4)&~3U;
    uint32_t first,last;TRY(ax_mem_read(d,address,&first));TRY(ax_mem_read(d,address+last_off,&last));
    if(first!=get32(start) || last!=get32(start+last_off))return ESP_ERR_INVALID_CRC;
    ESP_LOGI("AX900","Uploaded %s: %u bytes at %08lx",name,(unsigned)length,(unsigned long)address);
    return ESP_OK;
}
#define UPLOAD(name,address) upload(d,#name,address,name##_start,name##_end)

static esp_err_t apply_table(ax900_device_t *d,const uint8_t *table,size_t length) {
    for(size_t off=16;off<length;) {
        if(length-off<24)return ESP_ERR_INVALID_SIZE;
        uint32_t type=get32(table+off+16),count=get32(table+off+20);
        if(count>(length-off-24)/8)return ESP_ERR_INVALID_SIZE;
        const uint8_t *p=table+off+24;
        if(type!=6)for(uint32_t i=0;i<(type==0 && count>4?4:count);i++) {
            uint32_t value=get32(p+i*8+4);
            if(type==3 && count>=9 && i<9) {
                static const uint32_t btmode[]={1,0xffffffff,0,5,1,1500000,1,0,0x6f2f};value=btmode[i];
            }
            TRY(ax_mem_write(d,get32(p+i*8),value));
        }
        if(type==4)esp_rom_delay_us(500);
        off+=24+count*8;
    }
    return ESP_OK;
}
esp_err_t ax_load_firmware(ax900_device_t *d) {
    uint32_t value;TRY(ax_mem_read(d,0x40500000,&value));
    uint8_t chip=value>>16;
    ESP_LOGI("AX900","Chip register %08lx revision=%02x",(unsigned long)value,chip);
    if(chip!=3 && chip!=7)return ESP_ERR_NOT_SUPPORTED;
    const uint8_t *table=fw_patch_table_8800d80_u02_start;
    size_t length=fw_patch_table_8800d80_u02_end-table;
    if(length<64 || memcmp(table,"AICBT_PT_TAG",11) || get32(table+32)!=0)return ESP_ERR_INVALID_RESPONSE;
    uint32_t count=get32(table+36);
    if(count!=3 || count>(length-40)/8)return ESP_ERR_INVALID_SIZE;

    TRY(UPLOAD(fw_adid_8800d80_u02,get32(table+44)));
    TRY(UPLOAD(fw_patch_8800d80_u02,get32(table+52)));
    TRY(apply_table(d,table,length));

    TRY(UPLOAD(fmacfw_8800d80_u02,0x120000));
    TRY(ax_mem_write(d,0x40500048,0x1e0000));
    uint8_t req[8];put32(req,0x120000);put32(req+4,1);
    TRY(ax_command(d,0x40d,0,req,8,NULL,0,NULL));
    ax_pump(20);return ESP_OK;
}
