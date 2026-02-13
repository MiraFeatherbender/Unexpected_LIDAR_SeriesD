// Lightweight adapter: consumes dispatcher pointer messages for encoder/button
#ifndef UI_INPUT_ADAPTER_H
#define UI_INPUT_ADAPTER_H

#include "esp_err.h"

// Initialize the OLED indev adapter. Registers as dispatcher target TARGET_OLED_INDEV
esp_err_t ui_input_adapter_init(void);

// Deinitialize adapter (optional)
esp_err_t ui_input_adapter_deinit(void);

// Encoder callbacks
// page_cb: called for press+rotate events. Parameter is direction: +1 or -1.
// widget_cb: called for rotate-only events (dir +1/-1) and for press/release-without-rotate (dir==0).
typedef void (*ui_input_page_cb_t)(int8_t dir);
typedef void (*ui_input_widget_cb_t)(int8_t dir);

// Register callbacks (pass NULL to unregister)
void ui_input_set_page_callback(ui_input_page_cb_t cb);
void ui_input_set_widget_callback(ui_input_widget_cb_t cb);

#endif // UI_INPUT_ADAPTER_H
