#include "ui/pages/ui_page.h"
#include "ui/pages/ui_pages.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "ui_styles.h"
#include "ui_setting_item.h"
#include "ui_input_adapter.h"
#include "lvgl.h"

static const char *TAG = "ui_page_encoder_test";
static lv_obj_t *s_container = NULL;
static bool s_styles_initialized = false;
static ui_setting_item_t s_item;

static void ui_page_encoder_test_widget_cb(int8_t dir)
{
    lvgl_port_lock(0);
    ui_setting_item_handle_encoder(&s_item, dir, esp_timer_get_time() / 1000);

    lvgl_port_unlock();
}


static esp_err_t ui_page_encoder_test_init(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "encoder_test init");
    if (!parent) parent = lv_scr_act();
    if (!s_styles_initialized) {
        ui_styles_init();
        s_styles_initialized = true;
    }

    // create a simple container for encoder_test
    s_container = lv_obj_create(parent);
    if (s_container) {
        lv_obj_set_size(s_container, lv_obj_get_width(parent), lv_obj_get_height(parent) - 16);
        lv_obj_set_align(s_container, LV_ALIGN_BOTTOM_MID);
        lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(s_container, &ui_style_light_mode, 0);
    }

    ui_setting_item_config_t cfg = {
        .name = "Value",
        .value_type = UI_SETTING_VALUE_INT32,
        .initial_value = {.i32 = 0},
        .min_value = {.i32 = -200},
        .max_value = {.i32 = 200},
        .commit_policy = UI_SETTING_COMMIT_LIVE,
        .accel = {
            .base_step = 1.0f,
            .gain_k = 0.02f,
            .tau_ms = 250.0f,
            .accel_max = 6.0f,
        },
        .source = SOURCE_OLED_INDEV,
        .target = TARGET_LOG,
        .pool_type = DISPATCHER_POOL_STREAMING,
    };

    if (ui_setting_item_create_bar(&s_item, s_container, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "setting item init failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void ui_page_encoder_test_deinit(void)
{
    ESP_LOGI(TAG, "encoder_test deinit");
    if (s_container) {
        lv_obj_del(s_container);
        s_container = NULL;
    }
    ui_setting_item_destroy(&s_item);
}

static void ui_page_encoder_test_show(lv_obj_t *parent)
{
    (void)parent; /* widgets created in init(parent) */
    ui_setting_item_set_ignore_next_tap(&s_item, true);
    ui_input_set_widget_callback(ui_page_encoder_test_widget_cb);
}

static void ui_page_encoder_test_hide(void)
{
    ui_setting_item_commit(&s_item, UI_SETTING_COMMIT_REASON_PAGE_HIDE);
    ui_input_set_widget_callback(NULL);
}

const ui_page_t ui_page_ENCODER_TEST = {
    .id = UI_PAGE_ENCODER_TEST,
    .name = "Encoder_Test",
    .init = ui_page_encoder_test_init,
    .deinit = ui_page_encoder_test_deinit,
    .show = ui_page_encoder_test_show,
    .hide = ui_page_encoder_test_hide,
};
