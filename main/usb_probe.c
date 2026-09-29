#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "ax900.h"
static const char *TAG="TAB5_AX900";
static uint8_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg)
{
    uint8_t value;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev, &reg, 1, &value, 1, 1000));
    return value;
}

static void update_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t set, uint8_t clear)
{
    uint8_t before = read_reg(dev, reg);
    uint8_t data[] = {reg, (before | set) & (uint8_t)~clear};
    ESP_ERROR_CHECK(i2c_master_transmit(dev, data, sizeof(data), 1000));
    uint8_t after = read_reg(dev, reg);
    ESP_LOGI(TAG, "PI4IOE2 reg=0x%02x before=0x%02x after=0x%02x", reg, before, after);
    ESP_ERROR_CHECK(after == data[1] ? ESP_OK : ESP_FAIL);
}

static void enable_usb_power(void)
{
    // Tab5: GPIO31 SDA, GPIO32 SCL; PI4IOE2 (0x44), P3 = USB5V_EN.
    // Change only P3, preserving the charging and WLAN controls.
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 31,
        .scl_io_num = 32,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x44,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));
    update_reg(dev, 0x05, 0, 1 << 3);
    vTaskDelay(pdMS_TO_TICKS(1000));
    update_reg(dev, 0x05, 1 << 3, 0); // Cold-start the adapter for repeatable bring-up.
    update_reg(dev, 0x03, 1 << 3, 0); // 1 = output.
    update_reg(dev, 0x07, 0, 1 << 3); // Disable high impedance for P3.
    ESP_LOGI(TAG, "Tab5 USB-A 5V enable asserted");
    vTaskDelay(pdMS_TO_TICKS(500));
}

static void host_task(void *arg)
{
    while (true) {
        uint32_t flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
    }
}

static bool enum_filter(const usb_device_desc_t *desc, uint8_t *configuration) {
    *configuration=1;
    return true;
}
void app_main(void) {
    enable_usb_power();
    usb_host_config_t cfg={.intr_flags=ESP_INTR_FLAG_LEVEL1,.peripheral_map=1,.enum_filter_cb=enum_filter};
    ESP_ERROR_CHECK(usb_host_install(&cfg));
    xTaskCreate(host_task,"usb_host",4096,NULL,6,NULL);
    ESP_ERROR_CHECK(ax900_start());
}
