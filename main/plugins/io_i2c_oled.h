#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the I2C OLED example. If user_bus_cfg is NULL the code will
// attempt to find an existing bus via i2c_master_get_bus_handle (I2C_NUM_0/1).
esp_err_t io_i2c_oled_init(const i2c_master_bus_config_t *user_bus_cfg);
void io_i2c_oled_deinit(void);

#ifdef __cplusplus
}
#endif
