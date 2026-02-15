#include "ui_hello.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "ui/pages/ui_pages.h"
#include "esp_random.h"

static const char *TAG = "ui_hello";

LV_IMG_DECLARE(face_on_array);
LV_IMG_DECLARE(standby_face_array);
LV_IMG_DECLARE(blink_array);
LV_IMG_DECLARE(face_off_array);

#define HELLO_PERIODIC_TIMER_MS 650

typedef enum {
    HELLO_STEP_FACE_ON = 0,
    HELLO_STEP_STANDBY,
    HELLO_STEP_BLINK,
    HELLO_STEP_FACE_OFF,
    HELLO_STEP_NULL,
} hello_state_t;

static hello_state_t s_state = HELLO_STEP_NULL;
static bool s_busy = true;
static lv_timer_t *s_timer = NULL;
static bool s_step_pending = false;


static lv_obj_t *s_label = NULL;
static lv_obj_t *img;

static void hello_step(void);

static void hello_step_async_cb(void *user_data)
{
    (void)user_data;
    s_step_pending = false;
    hello_step();
}

static void hello_schedule_step(void)
{
    if (s_step_pending) return;
    s_step_pending = true;
    if (lv_async_call(hello_step_async_cb, NULL) != LV_RESULT_OK) {
        s_step_pending = false;
    }
}

static void hello_step(void){

    if (!img) return;
    if (s_busy) return;

    switch (s_state){
        case HELLO_STEP_FACE_ON:
            s_busy = true;
            lv_gif_set_src(img, &face_on_array);
            lv_gif_set_loop_count(img, 1);
            lv_gif_restart(img);
            s_state = HELLO_STEP_STANDBY;
            break;
        case HELLO_STEP_STANDBY:
            lv_gif_set_src(img, &standby_face_array);
            lv_gif_pause(img);
            s_busy = false;
            break;
        case HELLO_STEP_BLINK:
            s_busy = true;
            lv_gif_set_src(img, &blink_array);
            lv_gif_set_loop_count(img, 1);
            lv_gif_restart(img);
            s_state = HELLO_STEP_STANDBY;
            break;
        case HELLO_STEP_FACE_OFF:
            s_busy = true;
            lv_gif_set_src(img, &face_off_array);
            lv_gif_set_loop_count(img, 1);
            lv_gif_restart(img);
            s_state = HELLO_STEP_NULL;
            break;
        default:
            // Keep current frame when idle/null.
            s_busy = false;
            break;
    }
}

static void gif_pause_event_cb(lv_event_t *e)
{
    (void)e;
    if (!img) return;
    if (!s_busy) return;
    s_busy = false;
    hello_schedule_step();
    
};

static void hello_periodic_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!img) return;
    if (s_busy) return;
    if (s_state != HELLO_STEP_STANDBY) return;

    if ((esp_random() % 11U) <= 2U) {
        s_state = HELLO_STEP_BLINK;
        hello_schedule_step();
    }

}

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
    s_timer = lv_timer_create(hello_periodic_timer_cb, HELLO_PERIODIC_TIMER_MS, NULL);
    return ESP_OK;
}

static void ui_page_hello_deinit(void) {
    if (img) {
        lv_obj_del(img);
        img = NULL;
    }
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
}

static void ui_page_hello_show(lv_obj_t *parent)
{
    (void)parent; /* widgets created in init(parent) */
    if (s_timer) lv_timer_reset(s_timer);
    
    s_busy = false;
    s_state = HELLO_STEP_FACE_ON;
    hello_schedule_step();

}

static void ui_page_hello_hide(void) { 
    if (s_timer) lv_timer_reset(s_timer);
    
    s_busy = false;
    s_state = HELLO_STEP_FACE_OFF;
    hello_schedule_step();

}

const ui_page_t ui_page_HELLO = {
    .id = UI_PAGE_HELLO,
    .name = "Hello",
    .init = ui_page_hello_init,
    .deinit = ui_page_hello_deinit,
    .show = ui_page_hello_show,
    .hide = ui_page_hello_hide,
};

