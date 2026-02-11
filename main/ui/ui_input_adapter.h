// Lightweight adapter: consumes dispatcher pointer messages for encoder/button
#ifndef UI_INPUT_ADAPTER_H
#define UI_INPUT_ADAPTER_H

#include "esp_err.h"

// Initialize the OLED indev adapter. Registers as dispatcher target TARGET_OLED_INDEV
esp_err_t ui_input_adapter_init(void);

// Deinitialize adapter (optional)
esp_err_t ui_input_adapter_deinit(void);

#endif // UI_INPUT_ADAPTER_H
