#include "ui_input_adapter.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"

#include "dispatcher.h"
#include "dispatcher/dispatcher_pool.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui_hello.h"

static const char *TAG = "ui_input_adapter";

// Bitfield encoding helpers (upper nibble = dir, lower nibble = button state)
#define ENC_DIR_POS 0x10
#define ENC_DIR_ZERO 0x00
#define ENC_DIR_NEG  0x30

#define BTN_RELEASED_KEEP 0x03
#define BTN_PRESS_START   0x02
#define BTN_HELD          0x00
#define BTN_PRESS_END     0x01

static inline int8_t decode_dir(uint8_t b) {
    uint8_t d = (b >> 4) & 0xF;
    if (d == 0x1) return +1;
    if (d == 0x3) return -1;
    return 0;
}

static inline uint8_t decode_btn(uint8_t b) { return b & 0xF; }

typedef struct {
    lv_indev_t *indev;
    QueueHandle_t queue; // receives pool_msg_t * pointers
    TaskHandle_t task;
    volatile int32_t enc_accum;
    volatile uint8_t btn_state; // LV_INDEV_STATE_* value
} ui_input_ctx_t;

static ui_input_ctx_t s_ctx;

// Callbacks
static ui_input_page_cb_t s_page_cb = NULL;
static ui_input_widget_cb_t s_widget_cb = NULL;

void ui_input_set_page_callback(ui_input_page_cb_t cb) { s_page_cb = cb; }
void ui_input_set_widget_callback(ui_input_widget_cb_t cb) { s_widget_cb = cb; }

static void ui_input_indev_read(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    int32_t diff = __atomic_exchange_n(&s_ctx.enc_accum, 0, __ATOMIC_SEQ_CST);
    data->enc_diff = diff;
    uint8_t st = __atomic_load_n(&s_ctx.btn_state, __ATOMIC_SEQ_CST);
    data->state = (st == LV_INDEV_STATE_PRESSED) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void ui_input_worker(void *arg) {
    (void)arg;
    pool_msg_t *pmsg = NULL;
    for (;;) {
        if (xQueueReceive(s_ctx.queue, &pmsg, portMAX_DELAY) != pdTRUE) continue;

        const dispatcher_msg_ptr_t *pm = dispatcher_pool_get_msg_const(pmsg);
        if (pm && pm->data && pm->message_len > 0) {
            uint8_t b = pm->data[0];
            int8_t delta = decode_dir(b);
            uint8_t btn = decode_btn(b);

            // ESP_LOGI(TAG, "ui_input event: delta=%d btn=0x%02x raw=0x%02x", delta, btn, b);

            // Switch on the raw encoded byte `b` to handle precise combinations
            switch (b) {
                /* Rotation while held pressed -> page navigation (do not forward to LVGL) */
                case (ENC_DIR_POS | BTN_HELD): // 0x10
                    if (s_page_cb) s_page_cb(+1);
                    break;
                case (ENC_DIR_NEG | BTN_HELD): // 0x30
                    if (s_page_cb) s_page_cb(-1);
                    break;

                /* Rotation while released -> normal rotate-only for LVGL widgets */
                case (ENC_DIR_POS | BTN_RELEASED_KEEP): // 0x13
                    __atomic_add_fetch(&s_ctx.enc_accum, (int32_t)+1, __ATOMIC_SEQ_CST);
                    if (s_widget_cb) s_widget_cb(+1);
                    break;
                case (ENC_DIR_NEG | BTN_RELEASED_KEEP): // 0x33
                    __atomic_add_fetch(&s_ctx.enc_accum, (int32_t)-1, __ATOMIC_SEQ_CST);
                    if (s_widget_cb) s_widget_cb(-1);
                    break;

                /* Button transitions: press (0x02) and release (0x01) */
                case (ENC_DIR_ZERO | BTN_PRESS_END): // 0x01 press->release
                    if (s_widget_cb) s_widget_cb(0); // tap
                    __atomic_store_n(&s_ctx.btn_state, (uint8_t)LV_INDEV_STATE_RELEASED, __ATOMIC_SEQ_CST);
                    break;
                case (ENC_DIR_ZERO | BTN_PRESS_START): // 0x02 release->press
                    __atomic_store_n(&s_ctx.btn_state, (uint8_t)LV_INDEV_STATE_PRESSED, __ATOMIC_SEQ_CST);
                    break;

                default:
                    // Fallback: if direction present, forward to LVGL, else update state
                    if (delta != 0) {
                        __atomic_add_fetch(&s_ctx.enc_accum, (int32_t)delta, __ATOMIC_SEQ_CST);
                        if (s_widget_cb) s_widget_cb(delta > 0 ? +1 : -1);
                    } else {
                        // Unknown/other button state -> reflect released
                        __atomic_store_n(&s_ctx.btn_state, (uint8_t)LV_INDEV_STATE_RELEASED, __ATOMIC_SEQ_CST);
                    }
                    break;
            }

            // Wake LVGL so the read_cb will run promptly (LVGL9)
            if (s_ctx.indev) {
                lvgl_port_task_wake(LVGL_PORT_EVENT_TOUCH, s_ctx.indev);
            }
        }

        dispatcher_pool_msg_unref(pmsg);
        pmsg = NULL;
    }
}

esp_err_t ui_input_adapter_init(void) {
    memset(&s_ctx, 0, sizeof(s_ctx));

    s_ctx.queue = xQueueCreate(16, sizeof(pool_msg_t *));
    if (!s_ctx.queue) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    // Register pointer queue for dispatcher target
    dispatcher_register_ptr_queue(TARGET_OLED_INDEV, s_ctx.queue);

    BaseType_t ok = xTaskCreate(ui_input_worker, "ui_input_worker", 3072, NULL, 5, &s_ctx.task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        vQueueDelete(s_ctx.queue);
        dispatcher_register_ptr_queue(TARGET_OLED_INDEV, NULL);
        return ESP_ERR_NO_MEM;
    }

    // Create LVGL indev for encoder and wire read callback (LVGL v8 API)
    lvgl_port_lock(0);
    {
        lv_indev_t *indev = lv_indev_create();
        if (indev) {
            lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
            lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
            lv_indev_set_read_cb(indev, ui_input_indev_read);
            lv_indev_set_driver_data(indev, &s_ctx);
            /* Associate the indev with the default LVGL group so encoder
               rotations operate on focusable widgets assigned to that group. */
            lv_group_t *g = lv_group_get_default();
            if (!g) g = lv_group_create();
            if (g) lv_indev_set_group(indev, g);
            s_ctx.indev = indev;
        } else {
            ESP_LOGW(TAG, "lv_indev_create failed");
        }
    }
    lvgl_port_unlock();

    // Start with released state
    __atomic_store_n(&s_ctx.btn_state, (uint8_t)LV_INDEV_STATE_RELEASED, __ATOMIC_SEQ_CST);
    __atomic_store_n(&s_ctx.enc_accum, (int32_t)0, __ATOMIC_SEQ_CST);

    ESP_LOGI(TAG, "ui_input_adapter initialized");
    return ESP_OK;
}

esp_err_t ui_input_adapter_deinit(void) {
    // Unregister dispatcher queue
    dispatcher_register_ptr_queue(TARGET_OLED_INDEV, NULL);

    if (s_ctx.queue) {
        vQueueDelete(s_ctx.queue);
        s_ctx.queue = NULL;
    }

    if (s_ctx.indev) {
        lvgl_port_lock(0);
        lv_indev_delete(s_ctx.indev);
        lvgl_port_unlock();
        s_ctx.indev = NULL;
    }

    // Note: worker task will exit only if queue deleted and xQueueReceive fails;
    // it's acceptable to leave it or signal via task notification in future.

    ESP_LOGI(TAG, "ui_input_adapter deinitialized");
    return ESP_OK;
}
