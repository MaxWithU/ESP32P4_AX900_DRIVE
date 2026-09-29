#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef void *usb_host_client_handle_t;
typedef void *usb_device_handle_t;
typedef struct usb_transfer {
    uint8_t *data_buffer;
    int num_bytes,actual_num_bytes,status,flags;
    uint8_t bEndpointAddress;
    usb_device_handle_t device_handle;
    void *context;
    void (*callback)(struct usb_transfer *);
} usb_transfer_t;
typedef struct { uint16_t idVendor,idProduct; } usb_device_desc_t;
typedef struct __attribute__((packed)) { uint8_t bLength,bDescriptorType; uint16_t wTotalLength; uint8_t rest[5]; } usb_config_desc_t;
typedef struct {
    int event;
    struct { uint8_t address; } new_dev;
    struct { usb_device_handle_t dev_hdl; } dev_gone;
} usb_host_client_event_msg_t;
typedef struct { bool is_synchronous; unsigned max_num_event_msg; struct { void (*client_event_callback)(const usb_host_client_event_msg_t *,void *); } async; } usb_host_client_config_t;
#define USB_TRANSFER_STATUS_COMPLETED 0
#define USB_TRANSFER_STATUS_CANCELED 1
#define USB_TRANSFER_STATUS_STALL 2
#define USB_TRANSFER_FLAG_ZERO_PACK 1
#define USB_HOST_CLIENT_EVENT_NEW_DEV 1
#define USB_HOST_CLIENT_EVENT_DEV_GONE 2
esp_err_t usb_host_transfer_alloc(size_t n,int iso,usb_transfer_t **t);
esp_err_t usb_host_transfer_free(usb_transfer_t *t);
esp_err_t usb_host_transfer_submit(usb_transfer_t *t);
esp_err_t usb_host_transfer_submit_control(usb_host_client_handle_t c,usb_transfer_t *t);
esp_err_t usb_host_endpoint_halt(usb_device_handle_t d,uint8_t ep);
esp_err_t usb_host_endpoint_flush(usb_device_handle_t d,uint8_t ep);
esp_err_t usb_host_endpoint_clear(usb_device_handle_t d,uint8_t ep);
esp_err_t usb_host_client_handle_events(usb_host_client_handle_t c,unsigned ticks);
esp_err_t usb_host_device_open(usb_host_client_handle_t c,uint8_t address,usb_device_handle_t *d);
esp_err_t usb_host_device_close(usb_host_client_handle_t c,usb_device_handle_t d);
esp_err_t usb_host_get_device_descriptor(usb_device_handle_t d,const usb_device_desc_t **desc);
esp_err_t usb_host_get_active_config_descriptor(usb_device_handle_t d,const usb_config_desc_t **cfg);
esp_err_t usb_host_interface_claim(usb_host_client_handle_t c,usb_device_handle_t d,uint8_t i,uint8_t alt);
esp_err_t usb_host_interface_release(usb_host_client_handle_t c,usb_device_handle_t d,uint8_t i);
esp_err_t usb_host_client_register(const usb_host_client_config_t *cfg,usb_host_client_handle_t *c);
esp_err_t usb_host_client_deregister(usb_host_client_handle_t c);
void usb_print_config_descriptor(const usb_config_desc_t *cfg,void *printer);
