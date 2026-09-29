#pragma once
#include <cstdint>
struct lv_indev_t {};
struct lv_display_t {};
struct lv_group_t {};
enum {LV_INDEV_STATE_RELEASED,LV_INDEV_STATE_PRESSED,LV_INDEV_TYPE_KEYPAD};
struct lv_indev_data_t {uint32_t key=0;int state=0;bool continue_reading=false;};
lv_indev_t *lv_indev_create();
void lv_indev_set_type(lv_indev_t *,int);
void lv_indev_set_display(lv_indev_t *,lv_display_t *);
void lv_indev_set_read_cb(lv_indev_t *,void (*)(lv_indev_t *,lv_indev_data_t *));
void lv_indev_set_long_press_time(lv_indev_t *,int);
void lv_indev_reset(lv_indev_t *,void *);
void lv_indev_set_group(lv_indev_t *,lv_group_t *);
