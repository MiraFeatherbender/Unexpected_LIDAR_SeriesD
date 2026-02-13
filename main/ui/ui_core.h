#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize core UI subsystem. Creates root UI container and prepares page registry.
esp_err_t ui_core_init(void);
void ui_core_deinit(void);

// Pages are auto-registered via pages.def and discovered at build time.

// Show the page with the given id (page ids are arbitrary integers defined
// by each page module). Returns ESP_OK on success.
esp_err_t ui_core_show_page(int page_id);

// Convenience: show next/previous registered page
void ui_core_show_next(void);
void ui_core_show_prev(void);

#ifdef __cplusplus
}
#endif
