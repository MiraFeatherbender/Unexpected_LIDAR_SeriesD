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
static uint32_t current_color = 0;
static uint8_t current_brightness = 255;

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

                uint8_t plugin_id = msg.data[0];
                uint8_t r = msg.data[1];
                uint8_t g = msg.data[2];
                uint8_t b = msg.data[3];
                uint8_t brightness = msg.data[4];

                current_color = ums3_color(r, g, b);
                current_brightness = brightness;

                // Select plugin
                active_anim = rgb_plugins[plugin_id];

                if (active_anim) {
                    if (active_anim->begin)
                        active_anim->begin();

                    if (active_anim->set_color)
                        active_anim->set_color(current_color);

                    if (active_anim->set_brightness)
                        active_anim->set_brightness(current_brightness);
                }
            }
        }

        // Run active animation
        if (active_anim && active_anim->step)
            active_anim->step();

        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}