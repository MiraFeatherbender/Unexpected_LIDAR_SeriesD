/* ui_hello.c
 * Minimal UI module that creates a Hello World label on the active LVGL screen.
 */

#include "ui_hello.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui_hello";

static lv_obj_t *s_label = NULL;
static lv_timer_t *s_toggle_timer = NULL;

static void toggle_focus_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    if(!s_label) return;
    if(lv_obj_has_state(s_label, LV_STATE_FOCUSED)) lv_obj_clear_state(s_label, LV_STATE_FOCUSED);
    else lv_obj_add_state(s_label, LV_STATE_FOCUSED);
}

esp_err_t ui_hello_show(void)
{
    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "Failed to take LVGL lock");
        return ESP_ERR_INVALID_STATE;
    }

    lv_disp_set_rotation(lv_disp_get_default(), LV_DISPLAY_ROTATION_0);

    static lv_style_t invert_label_style;
    lv_style_init(&invert_label_style);

    /* Configure the style (use lv_style_set_* on lv_style_t) */
    lv_style_set_text_align(&invert_label_style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_bg_color(&invert_label_style, lv_color_black());
    lv_style_set_text_color(&invert_label_style, lv_color_white());
    lv_style_set_bg_opa(&invert_label_style, LV_OPA_COVER);

    static lv_style_t label_style;
    lv_style_init(&label_style);

    lv_style_set_text_align(&label_style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_bg_color(&label_style, lv_color_white());
    lv_style_set_text_color(&label_style, lv_color_black());
    lv_style_set_bg_opa(&label_style, LV_OPA_COVER);

    lv_obj_t *label = lv_label_create(lv_scr_act());
    if (!label) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "Failed to create label");
        return ESP_ERR_NO_MEM;
    }
    lv_label_set_text(label, "Hello World");

    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_add_style(label, &label_style, 0);
    /* Attach focused-state inverted style */
    lv_obj_add_style(label, &invert_label_style, LV_STATE_FOCUSED);

    /* keep handle and start 3s timer to toggle focus for testing */
    s_label = label;
    // if(!s_toggle_timer) s_toggle_timer = lv_timer_create(toggle_focus_cb, 3000, NULL);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "Hello UI created");
    return ESP_OK;
}

void ui_hello_toggle_invert(void)
{
    if (!s_label) return;
    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "ui_hello_toggle_invert: failed to take LVGL lock");
        return;
    }
    if (lv_obj_has_state(s_label, LV_STATE_FOCUSED)) lv_obj_clear_state(s_label, LV_STATE_FOCUSED);
    else lv_obj_add_state(s_label, LV_STATE_FOCUSED);
    lvgl_port_unlock();
}
