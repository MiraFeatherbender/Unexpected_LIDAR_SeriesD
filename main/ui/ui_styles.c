#include "ui_styles.h"

lv_style_t ui_style_dark_mode;
lv_style_t ui_style_light_mode;

static bool s_styles_initialized = false;

void ui_styles_init(void)
{
    if (s_styles_initialized) return;

    lv_style_init(&ui_style_dark_mode);
    lv_style_set_text_color(&ui_style_dark_mode, lv_color_black());
    lv_style_set_bg_color(&ui_style_dark_mode, lv_color_white());
    lv_style_set_bg_opa(&ui_style_dark_mode, LV_OPA_COVER);

    lv_style_init(&ui_style_light_mode);
    lv_style_set_text_color(&ui_style_light_mode, lv_color_white());
    lv_style_set_bg_color(&ui_style_light_mode, lv_color_black());
    lv_style_set_bg_opa(&ui_style_light_mode, LV_OPA_COVER);

    s_styles_initialized = true;
}
