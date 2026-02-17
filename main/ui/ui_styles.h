// Centralized UI styles (light/dark modes)
#pragma once

#include "lvgl.h"

// Initialize styles. Call once before creating UI elements.
extern lv_style_t ui_style_dark_mode;  // white bg, black text
extern lv_style_t ui_style_light_mode; // black bg, white text

void ui_styles_init(void);
