/*
 * UMSeriesD ESP-IDF Helper Library (adapted from Arduino version)
 * For Unexpected Maker Series[D] boards (e.g., ProS3)
 * This version uses ESP-IDF APIs and is not compatible with Arduino.
 */

#ifndef _UMSERIESD_IDF_H
#define _UMSERIESD_IDF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAX17048_REG_VCELL      = 0x02,
    MAX17048_REG_SOC        = 0x04,
    MAX17048_REG_MODE       = 0x06,
    MAX17048_REG_VERSION    = 0x08,
    MAX17048_REG_HIBRT      = 0x0A,
    MAX17048_REG_CONFIG     = 0x0C,
    MAX17048_REG_VALRT      = 0x14,
    MAX17048_REG_CRATE      = 0x16,
    MAX17048_REG_VRESET_ID  = 0x18,
    MAX17048_REG_STATUS     = 0x1A,
    MAX17048_REG_TABLE      = 0x40,
    MAX17048_REG_CMD        = 0xFE
} max17048_reg_t;

// Initialization
void ums3_begin(void);

// Power and LED control
void ums3_set_ldo2_power(bool on);
void ums3_set_pixel_power(bool on);
void ums3_set_pixel_color(uint8_t r, uint8_t g, uint8_t b);
void ums3_set_pixel_color32(uint32_t rgb);
void ums3_set_pixel_brightness(uint8_t brightness);
void ums3_write_pixel(void);
void ums3_set_blue_led(bool on);
void ums3_toggle_blue_led(void);

// Sensor and utility
float ums3_get_light_sensor_voltage(void);
void ums3_fg_setup(void); // TODO: Update for new I2C driver **done**
float ums3_get_battery_voltage(void);
uint8_t ums3_fg_version(void);
void ums3_set_antenna_external(bool state);
bool ums3_get_vbus_present(void);

// Static utility
uint32_t ums3_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t ums3_color_wheel(uint8_t pos);

// MAX17048 I2C helpers
void ums3_i2c_write_reg(max17048_reg_t reg);    // TODO: Update for new I2C driver
void ums3_i2c_write_reg16(max17048_reg_t reg, uint16_t data); // TODO: Update for new I2C driver
uint16_t ums3_i2c_read_reg16(max17048_reg_t reg); // TODO: Update for new I2C driver

#ifdef __cplusplus
}
#endif

#endif // _UMSERIESD_IDF_H
