/* io_i2c_oled.c
 * Adaptation of managed_component espressif__esp_lvgl_port example 'i2c_oled'
 * Reuses existing i2c_master_bus_handle_t when available using bus-detect loop
 */

#include "io_i2c_oled.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#if CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
#include "esp_lcd_sh1107.h"
#else
#include "esp_lcd_panel_vendor.h"
#endif

static const char *TAG = "io_i2c_oled";

#define I2C_HOST  0
#define EXAMPLE_I2C_HW_ADDR 0x3C
#define EXAMPLE_LCD_H_RES 128
#define EXAMPLE_LCD_V_RES 64

esp_err_t io_i2c_oled_init(const i2c_master_bus_config_t *user_bus_cfg)
{
    ESP_LOGI(TAG, "io_i2c_oled: init start");

    i2c_master_bus_handle_t i2c_bus = NULL;

    if (user_bus_cfg) {
        esp_err_t rc = i2c_new_master_bus(user_bus_cfg, &i2c_bus);
        if (rc != ESP_OK || !i2c_bus) {
            ESP_LOGW(TAG, "failed to create I2C master bus from provided config: 0x%X", rc);
            return rc != ESP_OK ? rc : ESP_ERR_NOT_FOUND;
        }
        ESP_LOGI(TAG, "created new I2C bus from config (port=%d SDA=%d SCL=%d)", user_bus_cfg->i2c_port, user_bus_cfg->sda_io_num, user_bus_cfg->scl_io_num);
    } else {
        const int AUTO_SETUP_RETRIES = 50;
        const int AUTO_SETUP_DELAY_MS = 200;
        int attempt = 0;
        while (attempt < AUTO_SETUP_RETRIES) {
            if (i2c_master_get_bus_handle(I2C_NUM_0, &i2c_bus) == ESP_OK) break;
            if (i2c_master_get_bus_handle(I2C_NUM_1, &i2c_bus) == ESP_OK) break;
            attempt++;
            if (attempt >= AUTO_SETUP_RETRIES) break;
            ESP_LOGI(TAG, "waiting for I2C bus (attempt %d/%d)", attempt+1, AUTO_SETUP_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(AUTO_SETUP_DELAY_MS));
        }
        if (!i2c_bus) {
            ESP_LOGW(TAG, "No existing I2C master bus found; not creating one");
            return ESP_ERR_NOT_FOUND;
        }
        ESP_LOGI(TAG, "Using existing I2C master bus handle for display");
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = EXAMPLE_I2C_HW_ADDR,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
        .dc_bit_offset = 6,
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
        .dc_bit_offset = 0,
        .flags = { .disable_control_phase = 1, },
#else
        .dc_bit_offset = 6,
#endif
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,0,0))
        .color_space = ESP_LCD_COLOR_SPACE_MONOCHROME,
#endif
    };

#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,3,0))
    esp_lcd_panel_ssd1306_config_t ssd1306_config = { .height = EXAMPLE_LCD_V_RES };
    panel_config.vendor_config = &ssd1306_config;
#endif
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#else
    // Default to SSD1306 path if no controller config defined
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,3,0))
    esp_lcd_panel_ssd1306_config_t ssd1306_config = { .height = EXAMPLE_LCD_V_RES };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#else
    ESP_LOGW(TAG, "No specific panel driver available for this IDF version");
#endif
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = true,
        .rotation = { .swap_xy = false, .mirror_x = true, .mirror_y = true },
        .flags = { .sw_rotate = false },
    };

    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Display LVGL Hello World");
    if (lvgl_port_lock(0)) {
        lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_0);
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Hello World");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "io_i2c_oled: init complete");
    return ESP_OK;
}

void io_i2c_oled_deinit(void)
{
    // No-op for now; cleanup handled by caller if necessary
}
