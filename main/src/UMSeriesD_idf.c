/*
 * UMSeriesD ESP-IDF Helper Library (adapted from Arduino version)
 * For Unexpected Maker Series[D] boards (e.g., ProS3)
 * This version uses ESP-IDF APIs and is not compatible with Arduino.
 */

#include "UMSeriesD_idf.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "led_strip.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// -------- Board/Feature Gating --------
#if CONFIG_UM_BOARD_FEATHERS3
    #define ALS_ADC_CHANNEL ADC1_GPIO4_CHANNEL
    #define ALS_ADC_PIN 4
    #define HAS_RGB 1
    #define HAS_VBUS_SENSE 1
    #define RF_SWITCH 41
#elif CONFIG_UM_BOARD_TINYS3
    #define HAS_RGB 1
    #define HAS_VBUS_SENSE 1
    #define RF_SWITCH 38
#elif CONFIG_UM_BOARD_PROS3
    #define HAS_RGB 1
    #define HAS_VBUS_SENSE 1
    #define RF_SWITCH 11
#elif CONFIG_UM_BOARD_EDGES3D
    #define RF_SWITCH 38
#else
    #error "The board you have selected is not compatible with the UMS3 helper library"
#endif

#define MAX17048_I2C_ADDR 0x36

static i2c_master_dev_handle_t max17048_dev_handle = NULL;

// -------- LED Strip Handle (if RGB present) --------
#if defined(HAS_RGB)
static led_strip_handle_t s_led_strip = NULL;
static uint8_t s_pixel_brightness = 255;
#endif

// -------- Begin Function --------
void ums3_begin(void) {
#if defined(HAS_RGB)
    // Enable power to RGB LED
    gpio_config_t ldo2_conf = {
        .pin_bit_mask = (1ULL << CONFIG_UM_RGB_PWR_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&ldo2_conf);
    gpio_set_level(CONFIG_UM_RGB_PWR_PIN, 1);

    // Initialize led_strip handle (RMT backend only)
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_UM_RGB_DATA_PIN,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE("UMSeriesD", "Failed to initialize led_strip (RMT): %d", err);
        s_led_strip = NULL;
    } else {
        led_strip_clear(s_led_strip);
    }
#endif

#if CONFIG_UM_BOARD_FEATHERS3
    // Blue LED as output
    gpio_config_t blue_led_conf = {
        .pin_bit_mask = (1ULL << CONFIG_UM_LED_BUILTIN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&blue_led_conf);
#endif

#if CONFIG_UM_BOARD_FEATHERS3
    // ADC attenuation for ALS
    adc1_config_channel_atten(ALS_ADC_CHANNEL, ADC_ATTEN_DB_11);
#endif

#if defined(HAS_VBUS_SENSE)
    // VBUS_SENSE as input
    gpio_config_t vbus_conf = {
        .pin_bit_mask = (1ULL << CONFIG_UM_VBUS_SENSE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&vbus_conf);
#endif

    // RF_SWITCH as output, default LOW (onboard antenna)
    gpio_config_t rf_conf = {
        .pin_bit_mask = (1ULL << RF_SWITCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rf_conf);
    gpio_set_level(RF_SWITCH, 0);

}

// -------- LDO2 Power Control --------
void ums3_set_ldo2_power(bool on) {
#if CONFIG_UM_BOARD_PROS3 || CONFIG_UM_BOARD_FEATHERS3
    gpio_set_level(CONFIG_UM_LDO2_PIN, on ? 1 : 0);
#else
    ESP_LOGE("UMSeriesD", "setLDO2Power not available on this board");
#endif
}

// -------- Pixel Power Control --------
void ums3_set_pixel_power(bool on) {
#if defined(HAS_RGB)
    gpio_set_level(CONFIG_UM_RGB_PWR_PIN, on ? 1 : 0);
#else
    ESP_LOGE("UMSeriesD", "setPixelPower not available on this board");
#endif
}

// -------- Pixel Brightness --------
void ums3_set_pixel_brightness(uint8_t brightness) {
#if defined(HAS_RGB)
    s_pixel_brightness = brightness;
#else
    ESP_LOGE("UMSeriesD", "setPixelBrightness not available on this board");
#endif
}

// -------- Pixel Color --------
void ums3_set_pixel_color(uint8_t r, uint8_t g, uint8_t b) {
#if defined(HAS_RGB)
    if (s_led_strip) {
        r = (r * s_pixel_brightness) / 255;
        g = (g * s_pixel_brightness) / 255;
        b = (b * s_pixel_brightness) / 255;
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
    } else {
        ESP_LOGE("UMSeriesD", "setPixelColor: led_strip not initialized");
    }
#else
    ESP_LOGE("UMSeriesD", "setPixelColor not available on this board");
#endif
}

void ums3_set_pixel_color32(uint32_t rgb) {
#if defined(HAS_RGB)
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    ums3_set_pixel_color(r, g, b);
#else
    ESP_LOGE("UMSeriesD", "setPixelColor (32-bit) not available on this board");
#endif
}

// -------- Blue LED Control --------
void ums3_set_blue_led(bool on) {
#if CONFIG_UM_BOARD_FEATHERS3
    gpio_set_level(CONFIG_UM_LED_BUILTIN_PIN, on ? 1 : 0);
#else
    ESP_LOGE("UMSeriesD", "setBlueLED not available on this board");
#endif
}

void ums3_toggle_blue_led(void) {
#if CONFIG_UM_BOARD_FEATHERS3
    int level = gpio_get_level(CONFIG_UM_LED_BUILTIN_PIN);
    gpio_set_level(CONFIG_UM_LED_BUILTIN_PIN, !level);
#else
    ESP_LOGE("UMSeriesD", "toggleBlueLED not available on this board");
#endif
}

// -------- Light Sensor Voltage --------
float ums3_get_light_sensor_voltage(void) {
#if CONFIG_UM_BOARD_FEATHERS3
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(adc_handle, ALS_ADC_CHANNEL, &chan_config);

    int raw = 0;
    adc_oneshot_read(adc_handle, ALS_ADC_CHANNEL, &raw);

    // Optionally, use calibration for more accurate voltage
    // For now, assume 3.3V reference and 12-bit ADC
    float voltage = (raw / 4095.0f) * 3.3f;

    adc_oneshot_del_unit(adc_handle);
    return voltage;
#else
    ESP_LOGE("UMSeriesD", "getLightSensorVoltage not available on this board");
    return 0.0f;
#endif
}

// -------- Fuel Gauge Setup --------
void ums3_fg_setup(void) {
    // I2C for battery gauge (MAX17048)
    // // TODO: Migrate the following to new driver/i2c_master.h API
    // i2c_config_t i2c_conf = {
    //     .mode = I2C_MODE_MASTER,
    //     .sda_io_num = CONFIG_UM_I2C_SDA_PIN,
    //     .scl_io_num = CONFIG_UM_I2C_SCL_PIN,
    //     .sda_pullup_en = GPIO_PULLUP_ENABLE,
    //     .scl_pullup_en = GPIO_PULLUP_ENABLE,
    //     .master.clk_speed = 400000
    // };
    // i2c_param_config(I2C_NUM_0, &i2c_conf); // <-- OLD I2C DRIVER USAGE
    // i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0); // <-- OLD I2C DRIVER USAGE
    // // (No gating needed, always available if I2C is present)

    // --- New I2C driver (driver/i2c_master.h) structure to fill in ---
    i2c_master_bus_config_t bus_config = {
        .i2c_port   = I2C_NUM_0,
        .scl_io_num = CONFIG_UM_I2C_SCL_PIN,
        .sda_io_num = CONFIG_UM_I2C_SDA_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    // Use bus_handle for all I2C operations
    // (No gating needed, always available if I2C is present)
    if (err != ESP_OK) {
        ESP_LOGE("UMSeriesD", "I2C bus initialization failed: %d", err);
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX17048_I2C_ADDR, // 0x36
        .scl_speed_hz = 400000,              // 400kHz
    };

    err = i2c_master_bus_add_device(bus_handle, &dev_config, &max17048_dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE("UMSeriesD", "I2C device initialization failed: %d", err);
    }
}

// -------- Battery Voltage --------
float ums3_get_battery_voltage(void) {
    uint16_t raw = ums3_i2c_read_reg16(MAX17048_REG_VCELL);
    // Direct Arduino adaptation: return ((float)i2c_read(MAX17048_REG::VCELL) * 78.125f / 1000000.f);
    return ((float)raw * 78.125f / 1000000.f);
}

// -------- Fuel Gauge Version --------
uint8_t ums3_fg_version(void) {
    uint16_t raw = ums3_i2c_read_reg16(MAX17048_REG_VERSION);
    return (uint8_t)raw;
}

// -------- Antenna Switch --------
void ums3_set_antenna_external(bool state) {
    gpio_set_level(RF_SWITCH, state ? 1 : 0);
}

// -------- VBUS Sense --------
bool ums3_get_vbus_present(void) {
#if defined(HAS_VBUS_SENSE)
    return gpio_get_level(CONFIG_UM_VBUS_SENSE_PIN) == 1;
#else
    ESP_LOGE("UMSeriesD", "getVbusPresent not available on this board");
    return false;
#endif
}

// -------- Color Utility --------
uint32_t ums3_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t ums3_color_wheel(uint8_t pos) {
    // Arduino logic: color wheel
    if (pos < 85) {
        return ums3_color(255 - pos * 3, pos * 3, 0);
    } else if (pos < 170) {
        pos -= 85;
        return ums3_color(0, 255 - pos * 3, pos * 3);
    } else {
        pos -= 170;
        return ums3_color(pos * 3, 0, 255 - pos * 3);
    }
}

// -------- MAX17048 I2C Helpers --------
void ums3_i2c_write_reg(max17048_reg_t reg) {
    uint8_t reg_addr = (uint8_t)reg;
    // TODO: Migrate i2c_master_write_to_device to new driver/i2c_master.h API
    esp_err_t err = i2c_master_transmit(
        max17048_dev_handle, &reg_addr, 1, 100 / portTICK_PERIOD_MS); // <-- OLD I2C DRIVER USAGE
    if (err != ESP_OK) {
        ESP_LOGE("UMSeriesD", "I2C write_reg failed: %d", err);
    }
}

void ums3_i2c_write_reg16(max17048_reg_t reg, uint16_t data) {
    uint8_t buf[3];
    buf[0] = (uint8_t)reg;
    buf[1] = (data & 0xFF00) >> 8;
    buf[2] = (data & 0x00FF);
    // TODO: Migrate i2c_master_write_to_device to new driver/i2c_master.h API
    esp_err_t err = i2c_master_transmit(
        max17048_dev_handle, buf, 3, 100 / portTICK_PERIOD_MS); // <-- OLD I2C DRIVER USAGE
    if (err != ESP_OK) {
        ESP_LOGE("UMSeriesD", "I2C write_reg16 failed: %d", err);
    }
}

uint16_t ums3_i2c_read_reg16(max17048_reg_t reg) {
    uint8_t reg_addr = (uint8_t)reg;
    uint8_t data[2] = {0};
    // TODO: Migrate i2c_master_write_read_device to new driver/i2c_master.h API
    esp_err_t err = i2c_master_transmit_receive(
        max17048_dev_handle, &reg_addr, 1, data, 2, 100 / portTICK_PERIOD_MS); // <-- OLD I2C DRIVER USAGE
    if (err != ESP_OK) {
        ESP_LOGE("UMSeriesD", "I2C read_reg16 failed: %d", err);
        return 0;
    }
    return ((uint16_t)data[0] << 8) | data[1];
}


