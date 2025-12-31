#include "io_rgb.h"
#include "dispatcher.h"
#include "UMSeriesD_idf.h"
#include "rgb_anim.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define RGB_CMD_QUEUE_LEN 10
#define RGB_TASK_STACK_SIZE 4096
#define RGB_TASK_PRIORITY 5

// Queue for incoming RGB commands
static QueueHandle_t rgb_cmd_queue = NULL;

// Plugin registry (dispatcher‑style)
static const rgb_anim_t *rgb_plugins[RGB_PLUGIN_MAX] = {0};

// Active plugin + current parameters
static const rgb_anim_t *active_anim = NULL;
static hsv_color_t current_hsv = {0, 0, 0};
static uint8_t current_brightness = 255;

// Track last command to avoid resetting animation unnecessarily
static uint8_t last_plugin_id = RGB_PLUGIN_MAX; // invalid sentinel
static hsv_color_t last_hsv = {0, 0, 0};
static uint8_t last_brightness = 255;
static bool has_last = false;

// HSV8 to RGB888 conversion (0-255 range)
enum { SRC_V = 0, SRC_P = 1, SRC_Q = 2, SRC_T = 3 };

static const uint8_t rgb_src[6][3] = {
    //  R       G       B
    { SRC_V, SRC_T, SRC_P }, // region 0
    { SRC_Q, SRC_V, SRC_P }, // region 1
    { SRC_P, SRC_V, SRC_T }, // region 2
    { SRC_P, SRC_Q, SRC_V }, // region 3
    { SRC_T, SRC_P, SRC_V }, // region 4
    { SRC_V, SRC_P, SRC_Q }, // region 5
};

void hsv8_to_rgb888(uint8_t h, uint8_t s, uint8_t v,
                      uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        // Achromatic (grey)
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    uint16_t h6 = (uint16_t)h * 6; // Scale to 0-1530
    uint8_t region = h6 >> 8; // 0-5
    uint8_t remainder = h6 & 0xFF; // 0-255

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    const uint8_t *src = rgb_src[region];
    uint8_t vals[4] = { v, p, q, t };

    *r = vals[src[0]];
    *g = vals[src[1]];
    *b = vals[src[2]];
}

// Registration API (mirrors dispatcher_register_handler)
void io_rgb_register_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin)
{
    if (id < RGB_PLUGIN_MAX)
        rgb_plugins[id] = plugin;
}

// Forward declaration
static void io_rgb_task(void *arg);

// Dispatcher handler — messages routed TO RGB
static void io_rgb_dispatcher_handler(const dispatcher_msg_t *msg)
{
    // Non-blocking enqueue
    xQueueSend(rgb_cmd_queue, msg, 0);
}

void io_rgb_init(void)
{
    // Create command queue
    rgb_cmd_queue = xQueueCreate(RGB_CMD_QUEUE_LEN, sizeof(dispatcher_msg_t));

    // Register with dispatcher
    dispatcher_register_handler(TARGET_RGB, io_rgb_dispatcher_handler);

    // Initialize RGB hardware
    ums3_set_pixel_brightness(current_brightness);
    ums3_set_pixel_color(0, 0, 0);

    // Start RGB task
    xTaskCreate(io_rgb_task,
                "io_rgb_task",
                RGB_TASK_STACK_SIZE,
                NULL,
                RGB_TASK_PRIORITY,
                NULL);
}

static void io_rgb_task(void *arg)
{
    dispatcher_msg_t msg;

    while (1) {

        // Handle incoming unified RGB command
        if (xQueueReceive(rgb_cmd_queue, &msg, 0) == pdTRUE) {

            if (msg.message_len >= 5) {

                // Extract HSV from message
                uint8_t plugin_id = msg.data[0];
                uint8_t h = msg.data[1];
                uint8_t s = msg.data[2];
                uint8_t v = msg.data[3];
                uint8_t brightness = msg.data[4];

                hsv_color_t new_hsv = {h, s, v};
                uint8_t new_brightness = brightness;

                bool plugin_changed = !has_last || (plugin_id != last_plugin_id);
                bool params_changed =
                    !has_last ||
                    (new_hsv.h != last_hsv.h) ||
                    (new_hsv.s != last_hsv.s) ||
                    (new_hsv.v != last_hsv.v) ||
                    (new_brightness != last_brightness);

                // Select plugin
                active_anim = rgb_plugins[plugin_id];

                if (active_anim) {

                    // Only reset animation when plugin changes
                    if (plugin_changed && active_anim->begin)
                        active_anim->begin();

                    // Always push updated parameters when they change
                    if (params_changed) {
                        if (active_anim->set_color)
                            active_anim->set_color(new_hsv);

                        if (active_anim->set_brightness)
                            active_anim->set_brightness(new_brightness);
                    }
                }

                // Commit new state
                current_hsv = new_hsv;
                current_brightness = new_brightness;
                last_plugin_id = plugin_id;
                last_hsv = new_hsv;
                last_brightness = new_brightness;
                has_last = true;
            }
        }

        // Run active animation and get HSV output
        hsv_color_t out_hsv = current_hsv;
        if (active_anim && active_anim->step)
            active_anim->step(&out_hsv);

        // Convert HSV to RGB and output
        uint8_t r, g, b;
        hsv8_to_rgb888(out_hsv.h, out_hsv.s, out_hsv.v, &r, &g, &b);
        ums3_set_pixel_color(r, g, b);

        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}