#pragma once
#include "ax900.h"
#include "usb/usb_host.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
typedef struct ax900_device {
    usb_device_handle_t usb;
    usb_transfer_t *rx;
    uint8_t in_ep, out_ep, interface, vif;
    bool rx_pending, gone, stopping, reply_done, scan_done;
    uint8_t scan_result;
    esp_err_t reply_error;
    uint16_t waiting_id;
    uint8_t reply[2048];
    size_t reply_len;
    uint8_t mac[6];
    bool runtime;
} ax900_device_t;
static inline uint16_t get16(const void *p) {const uint8_t *b=p; return b[0]|(uint16_t)b[1]<<8;}
static inline uint32_t get32(const void *p) {const uint8_t *b=p; return get16(b)|(uint32_t)get16(b+2)<<16;}
static inline void put16(void *p,uint16_t v) {uint8_t *b=p;b[0]=v;b[1]=v>>8;}
static inline void put32(void *p,uint32_t v) {uint8_t *b=p;put16(b,v);put16(b+2,v>>16);}
#define TRY(expr) do {esp_err_t _e=(expr); if(_e!=ESP_OK) {ESP_LOGE("AX900", "%s: %s (line %d)",#expr,esp_err_to_name(_e),__LINE__);return _e;}} while(0)
esp_err_t ax_command(ax900_device_t *d,uint16_t req,uint16_t cfm,const void *p,size_t n,void *r,size_t cap,size_t *got);
esp_err_t ax_mem_read(ax900_device_t *d,uint32_t address,uint32_t *value);
esp_err_t ax_mem_write(ax900_device_t *d,uint32_t address,uint32_t value);
esp_err_t ax_load_firmware(ax900_device_t *d);
esp_err_t ax_runtime_init(ax900_device_t *d);
esp_err_t ax_scan(ax900_device_t *d);
void ax_message(ax900_device_t *d,uint16_t id,const uint8_t *p,size_t n);
void ax_status(const char *text);
void ax_set_ready(bool supports_5ghz);
void ax_pump(unsigned ms);
