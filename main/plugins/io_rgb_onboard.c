#include "io_rgb.h"
#include "dispatcher.h"
#include "dispatcher_module.h"
#include "UMSeriesD_idf.h"
#include "rgb_anim.h"
#include "rgb_core.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define RGB_CMD_QUEUE_LEN 8
#define RGB_TASK_STACK_SIZE 4096
#define RGB_TASK_PRIORITY 5

static void io_rgb_process_msg(const dispatcher_msg_t *msg);
static void io_rgb_step_frame(void);

static dispatcher_module_t io_rgb_mod = {
    .name = "io_rgb_task",
    .target = TARGET_RGB_ONBOARD,
    .queue_len = RGB_CMD_QUEUE_LEN,
    .stack_size = RGB_TASK_STACK_SIZE,
    .task_prio = RGB_TASK_PRIORITY,
    .process_msg = io_rgb_process_msg,
    .step_frame = io_rgb_step_frame,
    .step_ms = 33,
    .queue = NULL,
    .next_step = 0
};

static uint8_t anim_brightness = 255;
static uint8_t anim_phase_u8 = 0;


void io_rgb_set_anim_brightness(uint8_t b)
{
    anim_brightness = b;
}


void io_rgb_init(void)
{
    // Initialize RGB hardware
    ums3_set_pixel_brightness(anim_brightness);
    ums3_set_pixel_color(0, 0, 0);    
    rgb_core_set_phase_ptr(&anim_phase_u8);
    
    if (dispatcher_module_start(&io_rgb_mod) != pdTRUE) {
        ESP_LOGE("io_rgb", "Failed to start dispatcher module for io_rgb");
        return;
    }
}



static uint64_t priority = 0; // Bitfield to track message source priority


static void default_action(const dispatcher_msg_t *msg)
{
    if (!msg || msg->message_len < 5) return;
    uint8_t plugin_id = msg->data[0];
    uint8_t h = msg->data[1];
    uint8_t s = msg->data[2];
    uint8_t v = msg->data[3];
    uint8_t brightness = msg->data[4];
    rgb_core_apply_command(plugin_id, h, s, v, brightness);
}

static TickType_t rest_off_until = 0;

static void io_rgb_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg) return;
    priority |= (1ULL << msg->source);

    if ((priority >> (msg->source + 1)) != 0) {
        // Higher priority source active, ignore this message
        return;
    }

    switch (msg->source) {
        case SOURCE_REST:
            if (msg->data[0] == RGB_PLUGIN_OFF) {
                rest_off_until = xTaskGetTickCount() + pdMS_TO_TICKS(5000); // 5 seconds off
            }
            default_action(msg);
            break;
        default:
            default_action(msg);
            break;
    }
}

static void io_rgb_step_frame(void)
{
    // Clear REST priority if timeout elapsed
    if (rest_off_until != 0 && xTaskGetTickCount() >= rest_off_until) {
        priority &= ~(1ULL << SOURCE_REST);
        rest_off_until = 0;
    }

    rgb_color_t out_rgb = {0, 0, 0};
    if (rgb_core_step_rgb(&out_rgb)) {
        ums3_set_pixel_brightness(anim_brightness);
        ums3_set_pixel_color(out_rgb.r, out_rgb.g, out_rgb.b);
    }
}
