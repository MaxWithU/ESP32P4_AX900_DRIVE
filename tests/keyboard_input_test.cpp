// SPDX-License-Identifier: MIT
// Exercise the actual ESP-IDF/LVGL adapter using synthetic, non-sensitive events.
#include "tab5_keyboard.h"
#include <bsp/m5stack_tab5.h>
#include <cassert>
#include <deque>
#include <cstdio>
static lv_indev_t input;static void (*reader)(lv_indev_t *,lv_indev_data_t *);
static std::deque<uint8_t> events;static bool attached=true;static int64_t now=0;
lv_indev_t *lv_indev_create(){return &input;}
void lv_indev_set_type(lv_indev_t *,int t){assert(t==LV_INDEV_TYPE_KEYPAD);}
void lv_indev_set_display(lv_indev_t *,lv_display_t *){}
void lv_indev_set_read_cb(lv_indev_t *,void (*fn)(lv_indev_t *,lv_indev_data_t *)){reader=fn;}
void lv_indev_set_long_press_time(lv_indev_t *,int){}
void lv_indev_reset(lv_indev_t *,void *){}
void lv_indev_set_group(lv_indev_t *,lv_group_t *){}
int64_t esp_timer_get_time(){return now;}
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c,i2c_master_bus_handle_t *h){
    assert(c->i2c_port==LP_I2C_NUM_0 && c->sda_io_num==0 && c->scl_io_num==1);*h=&input;return ESP_OK;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,const i2c_device_config_t *c,i2c_master_dev_handle_t *h){
    assert(c->device_address==0x6d && c->scl_speed_hz==100000);*h=&input;return ESP_OK;
}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t){return ESP_OK;}
esp_err_t i2c_master_probe(i2c_master_bus_handle_t,uint8_t a,int timeout){assert(a==0x6d && timeout==10);return attached?ESP_OK:ESP_FAIL;}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t,const uint8_t *reg,size_t n,uint8_t *out,size_t len,int timeout){
    assert(n==1 && len==1 && timeout==10);if(!attached)return ESP_FAIL;
    if(*reg==0xff)*out=0x6d;else if(*reg==0xfe)*out=1;else {assert(*reg==0x20);*out=events.empty()?0xff:events.front();if(!events.empty())events.pop_front();}return ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t,const uint8_t *bytes,size_t n,int timeout){
    assert(n==2 && timeout==10);if(!attached)return ESP_FAIL;if(bytes[0]==2)events.clear();return ESP_OK;
}
static lv_indev_data_t read(){lv_indev_data_t d;reader(&input,&d);return d;}
static void expect(uint32_t key,int state){auto d=read();assert(d.key==key && d.state==state);}
int main(){
    assert(tab5_keyboard_init(nullptr)==&input);read();assert(tab5_keyboard_connected());
    events={0xa1,0xa2,0x21,0x22}; // q down, w down, q up, w up
    expect('q',LV_INDEV_STATE_PRESSED);expect('q',LV_INDEV_STATE_RELEASED);
    expect('w',LV_INDEV_STATE_PRESSED);expect('w',LV_INDEV_STATE_RELEASED);
    events={0xa1};expect('q',LV_INDEV_STATE_PRESSED);expect('q',LV_INDEV_STATE_PRESSED);
    attached=false;assert(read().state==LV_INDEV_STATE_RELEASED && !tab5_keyboard_connected());
    attached=true;now=2000000;read();assert(tab5_keyboard_connected());
    events={0xa1,0xa2};tab5_keyboard_set_group(nullptr);assert(events.empty() && read().state==LV_INDEV_STATE_RELEASED);
    puts("Keyboard adapter: dedicated pins, overlapping keys, held keys, unplug release, reconnect and focus queue clearing passed");
}
