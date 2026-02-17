#include "ui/pages/ui_page.h"
#include "ui/pages/ui_pages.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "ui_dispatch_bridge.h"
#include "ui_styles.h"
#include "ui_encoder_accel.h"
#include "ui_input_adapter.h"
#include "lvgl.h"

static const char *TAG = "ui_page_encoder_test";
static lv_obj_t *s_container = NULL;
static lv_obj_t *s_name_label = NULL;
static lv_obj_t *s_value_label = NULL;
static int32_t s_value = 0;
static bool s_selected = false;
static bool s_ignore_next_tap = false;
static bool s_styles_initialized = false;
static lv_subject_t s_value_subject;
static bool s_value_subject_initialized = false;
static lv_subject_t s_selected_subject;
static bool s_selected_subject_initialized = false;

static ui_encoder_accel_t s_label_accel = {
    .base_step = 1.0f,
    .gain_k = 0.02f,
    .tau_ms = 250.0f,
    .accel_max = 6.0f,
    .accum = 0.0f,
    .residual = 0.0f,
    .last_ms = 0,
};

static void ui_page_encoder_test_commit_value(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "UI commit Value=%ld", (long)s_value);
    if (ui_dispatch_bridge_send_text(SOURCE_OLED_INDEV, TARGET_LOG, buf) != ESP_OK) {
        ESP_LOGW(TAG, "commit send failed");
    }
}

static void ui_page_encoder_test_widget_cb(int8_t dir)
{
    lvgl_port_lock(0);
    if (!s_name_label || !s_value_label) {
        lvgl_port_unlock();
        return;
    }

    if (dir == 0) {
        if (s_ignore_next_tap) {
            s_ignore_next_tap = false;
            lvgl_port_unlock();
            return;
        }
        s_selected = !s_selected;
        if (s_selected_subject_initialized) lv_subject_set_int(&s_selected_subject, s_selected ? 1 : 0);
        if (!s_selected) {
            ui_encoder_accel_reset(&s_label_accel);
            ui_page_encoder_test_commit_value();
        }
    } else if (s_selected) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        s_value += ui_encoder_accel_apply(&s_label_accel, dir, now_ms);
        if (s_value_subject_initialized) lv_subject_set_int(&s_value_subject, s_value);
    }

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
        lv_obj_add_style(s_container, &ui_style_dark_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    s_value = 0;

    s_name_label = lv_label_create(s_container);
    if (s_name_label) {
        lv_obj_align(s_name_label, LV_ALIGN_CENTER, 0, -10);
        lv_label_set_text(s_name_label, "Value");
        lv_obj_add_style(s_name_label, &ui_style_dark_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_style(s_name_label, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_USER_1);
        lv_subject_init_int(&s_selected_subject, 0);
        s_selected_subject_initialized = true;
        lv_obj_bind_state_if_eq(s_name_label, &s_selected_subject, LV_STATE_USER_1, 1);
    }

    s_value_label = lv_label_create(s_container);
    if (s_value_label) {
        lv_obj_align(s_value_label, LV_ALIGN_CENTER, 0, 10);
        lv_obj_add_style(s_value_label, &ui_style_dark_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_subject_init_int(&s_value_subject, s_value);
        s_value_subject_initialized = true;
        lv_label_bind_text(s_value_label, &s_value_subject, "%d");
    }

    s_selected = false;
    ui_encoder_accel_reset(&s_label_accel);
    if (s_selected_subject_initialized) lv_subject_set_int(&s_selected_subject, 0);
    if (s_value_subject_initialized) lv_subject_set_int(&s_value_subject, s_value);
    return ESP_OK;
}

static void ui_page_encoder_test_deinit(void)
{
    ESP_LOGI(TAG, "encoder_test deinit");
    if (s_container) {
        lv_obj_del(s_container);
        s_container = NULL;
        s_name_label = NULL;
        s_value_label = NULL;
    }
    if (s_value_subject_initialized) {
        lv_subject_deinit(&s_value_subject);
        s_value_subject_initialized = false;
    }
    if (s_selected_subject_initialized) {
        lv_subject_deinit(&s_selected_subject);
        s_selected_subject_initialized = false;
    }
}

static void ui_page_encoder_test_show(lv_obj_t *parent)
{
    (void)parent; /* widgets created in init(parent) */
    s_ignore_next_tap = true;
    ui_input_set_widget_callback(ui_page_encoder_test_widget_cb);
}

static void ui_page_encoder_test_hide(void)
{
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
