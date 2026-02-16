#include "ui/pages/ui_page.h"
#include "ui/pages/ui_pages.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "ui_styles.h"
#include "lvgl.h"

static const char *TAG = "ui_page_encoder_test";
static lv_obj_t *s_container = NULL;
static lv_obj_t *encoder_test_label = NULL;


static esp_err_t ui_page_encoder_test_init(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "encoder_test init");
    if (!parent) parent = lv_scr_act();
    // create a simple container for encoder_test
    s_container = lv_obj_create(parent);
    if (s_container) {
        lv_obj_set_size(s_container, lv_obj_get_width(parent), lv_obj_get_height(parent) - 16);
        lv_obj_set_align(s_container, LV_ALIGN_BOTTOM_MID);
        lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(s_container, &ui_style_dark_mode, 0);
    }
    
    encoder_test_label = lv_label_create(s_container);
    if (encoder_test_label) {
        lv_obj_set_align(encoder_test_label, LV_ALIGN_CENTER);
        lv_obj_add_style(encoder_test_label, &ui_style_dark_mode, 0);
    }
    return ESP_OK;
}

static void ui_page_encoder_test_deinit(void)
{
    ESP_LOGI(TAG, "encoder_test deinit");
    if (s_container) {
        lv_obj_del(s_container);
        s_container = NULL;
        encoder_test_label = NULL;
    }
}

static void ui_page_encoder_test_show(lv_obj_t *parent)
{
    (void)parent; /* widgets created in init(parent) */
    
    lv_label_set_text(encoder_test_label, "Encoder_Test page");
}

static void ui_page_encoder_test_hide(void)
{
    /* stop timers or animations here if any; widgets are deleted in deinit() */
}

const ui_page_t ui_page_ENCODER_TEST = {
    .id = UI_PAGE_ENCODER_TEST,
    .name = "Encoder_Test",
    .init = ui_page_encoder_test_init,
    .deinit = ui_page_encoder_test_deinit,
    .show = ui_page_encoder_test_show,
    .hide = ui_page_encoder_test_hide,
};
