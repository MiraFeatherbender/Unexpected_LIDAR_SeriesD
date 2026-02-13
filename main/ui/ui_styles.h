// Centralized UI styles (light/dark modes)
#pragma once

#include "lvgl.h"

// Initialize styles. Call once before creating UI elements.
// Simple header-only dark/light styles for UI elements
static lv_style_t ui_style_dark_mode;  // white bg, black text
static lv_style_t ui_style_light_mode; // black bg, white text

static inline void ui_styles_init(void)
{
	lv_style_init(&ui_style_dark_mode);
	lv_style_set_text_color(&ui_style_dark_mode, lv_color_black());
	lv_style_set_bg_color(&ui_style_dark_mode, lv_color_white());
	lv_style_set_bg_opa(&ui_style_dark_mode, LV_OPA_COVER);

	lv_style_init(&ui_style_light_mode);
	lv_style_set_text_color(&ui_style_light_mode, lv_color_white());
	lv_style_set_bg_color(&ui_style_light_mode, lv_color_black());
	lv_style_set_bg_opa(&ui_style_light_mode, LV_OPA_COVER);
}
