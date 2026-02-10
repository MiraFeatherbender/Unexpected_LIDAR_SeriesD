#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Create the simple Hello UI on the active LVGL screen.
// Safe to call after `io_i2c_oled_init()` and LVGL is initialized.
esp_err_t ui_hello_show(void);

#ifdef __cplusplus
}
#endif
