/* ui_hello.c
 * Minimal UI module that creates a Hello World label on the active LVGL screen.
 */

#include "ui_hello.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "ui/pages/ui_pages.h"

static const char *TAG = "ui_hello";

static lv_obj_t *s_label = NULL;
lv_obj_t *img;

static void gif_pause_event_cb(lv_event_t *e)
{
    lv_obj_t *gif = lv_event_get_target(e);
    lv_gif_pause(gif);
    
};

esp_err_t ui_hello_show(void)
{
    ESP_LOGI(TAG, "ui_hello_show: no-op (widgets removed)");
    return ESP_OK;
}

// Page descriptor glue so the hello module can be used as a ui_page
static esp_err_t ui_page_hello_init(void) { return ESP_OK; }
static void ui_page_hello_deinit(void) { }
static void ui_page_hello_show(lv_obj_t *parent)
{
    LV_IMG_DECLARE(face_on_array);

    if (!parent) parent = lv_scr_act();

    img = lv_gif_create(parent);
        lv_obj_add_event_cb(img, gif_pause_event_cb, LV_EVENT_READY, NULL);
        lv_gif_set_color_format(img, LV_COLOR_FORMAT_ARGB8888);
        lv_gif_set_src(img, &face_on_array);
        lv_obj_align(img, LV_ALIGN_BOTTOM_MID, 0, 0);

}

static void ui_page_hello_hide(void) { /* no-op */ }

const ui_page_t ui_page_HELLO = {
    .id = UI_PAGE_HELLO,
    .name = "Hello",
    .init = ui_page_hello_init,
    .deinit = ui_page_hello_deinit,
    .show = ui_page_hello_show,
    .hide = ui_page_hello_hide,
};

void ui_hello_toggle_invert(void) { /* no-op; widgets removed */ }
