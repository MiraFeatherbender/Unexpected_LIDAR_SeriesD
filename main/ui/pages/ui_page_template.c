#include "ui/pages/ui_page_template.h"
#include "ui/pages/ui_pages.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "ui_styles.h"
#include "lvgl.h"

static const char *TAG = "ui_page_template";
static lv_obj_t *s_container = NULL;
static lv_obj_t *temp_label = NULL;


esp_err_t ui_page_template_init(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "template init");
    if (!parent) parent = lv_scr_act();
    // create a simple container for template
    s_container = lv_obj_create(parent);
    if (s_container) {
        lv_obj_set_size(s_container, lv_obj_get_width(parent), lv_obj_get_height(parent) - 16);
        lv_obj_set_align(s_container, LV_ALIGN_BOTTOM_MID);
        lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(s_container, &ui_style_dark_mode, 0);
    }
    
    temp_label = lv_label_create(s_container);
    if (temp_label) {
        lv_obj_set_align(temp_label, LV_ALIGN_CENTER);
        lv_obj_add_style(temp_label, &ui_style_dark_mode, 0);
        lv_label_set_text(temp_label, "Template page");
    }
    return ESP_OK;
}

void ui_page_template_deinit(void)
{
    ESP_LOGI(TAG, "template deinit");
    if (s_container) {
        lv_obj_del(s_container);
        s_container = NULL;
        temp_label = NULL;
    }
}

void ui_page_template_show(lv_obj_t *parent)
{
    (void)parent; /* widgets created in init(parent) */
}

void ui_page_template_hide(void)
{
    /* stop timers or animations here if any; widgets are deleted in deinit() */
}

const ui_page_t ui_page_TEMPLATE = {
    .id = UI_PAGE_TEMPLATE,
    .name = "Template",
    .init = ui_page_template_init,
    .deinit = ui_page_template_deinit,
    .show = ui_page_template_show,
    .hide = ui_page_template_hide,
};
