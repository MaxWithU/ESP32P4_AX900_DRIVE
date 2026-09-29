#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_err.h"
using i2c_master_dev_handle_t=void *;
using i2c_master_bus_handle_t=void *;
enum {LP_I2C_NUM_0=2,GPIO_NUM_0=0,GPIO_NUM_1=1,LP_I2C_SCLK_DEFAULT=1,I2C_ADDR_BIT_LEN_7=7};
struct i2c_master_bus_config_t {int i2c_port,sda_io_num,scl_io_num,lp_source_clk;struct {bool enable_internal_pullup;} flags;};
struct i2c_device_config_t {int dev_addr_length,device_address,scl_speed_hz;};
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *,i2c_master_bus_handle_t *);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,const i2c_device_config_t *,i2c_master_dev_handle_t *);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t,const uint8_t *,size_t,uint8_t *,size_t,int);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t,const uint8_t *,size_t,int);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t,uint8_t,int);
