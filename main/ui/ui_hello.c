#include "ui_hello.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "ui/pages/ui_pages.h"

static const char *TAG = "ui_hello";

LV_IMG_DECLARE(face_on_array);
LV_IMG_DECLARE(standby_face_array);
LV_IMG_DECLARE(blink_array);
LV_IMG_DECLARE(face_off_array);

typedef enum {
    HELLO_STEP_FACE_ON = 0,
    HELLO_STEP_STANDBY,
    HELLO_STEP_BLINK,
    HELLO_STEP_FACE_OFF,
    HELLO_STEP_NULL,
} hello_state_t;

static hello_state_t s_state = HELLO_STEP_NULL;
static bool s_busy = true;

static lv_obj_t *s_label = NULL;
lv_obj_t *img;

static void hello_step(void){

    if(s_busy) return;
    lv_gif_pause(img);

    switch (s_state){
        case HELLO_STEP_FACE_ON:
            lv_gif_set_src(img, &face_on_array);
            s_state = HELLO_STEP_BLINK;
            break;
        case HELLO_STEP_STANDBY:
            lv_gif_set_src(img, &standby_face_array);
            s_state = HELLO_STEP_BLINK;
            break;
        case HELLO_STEP_BLINK:
            lv_gif_set_src(img, &blink_array);
            s_state = HELLO_STEP_FACE_OFF;
            break;
        case HELLO_STEP_FACE_OFF:
            lv_gif_set_src(img, &face_off_array);
            s_state = HELLO_STEP_NULL;
            break;
        default:
            lv_gif_set_src(img, NULL);
            break;
    }

    s_busy = true;
    lv_gif_set_loop_count(img, 1);
    lv_gif_restart(img);

    return;

}

static void gif_pause_event_cb(lv_event_t *e)
{
    (void)e;
    s_busy = false;
    hello_step();
    
};

// Page descriptor glue so the hello module can be used as a ui_page
static esp_err_t ui_page_hello_init(lv_obj_t *parent)
{

    if (!parent) parent = lv_scr_act();

    img = lv_gif_create(parent);
    if (img) {
        lv_obj_add_event_cb(img, gif_pause_event_cb, LV_EVENT_READY, NULL);
        lv_gif_set_color_format(img, LV_COLOR_FORMAT_ARGB8888);
        lv_obj_align(img, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    return ESP_OK;
}

static void ui_page_hello_deinit(void) {
    if (img) {
        lv_obj_del(img);
        img = NULL;
    }
}

static void ui_page_hello_show(lv_obj_t *parent)
{
    (void)parent; /* widgets created in init(parent) */
    
    s_busy = false;
    s_state = HELLO_STEP_FACE_ON;
    hello_step();
        // lv_gif_set_src(img, &face_on_array);
        // lv_gif_set_loop_count(img, 1);

}

static void ui_page_hello_hide(void) { /* stop timers/animations when we add them */ }

const ui_page_t ui_page_HELLO = {
    .id = UI_PAGE_HELLO,
    .name = "Hello",
    .init = ui_page_hello_init,
    .deinit = ui_page_hello_deinit,
    .show = ui_page_hello_show,
    .hide = ui_page_hello_hide,
};

