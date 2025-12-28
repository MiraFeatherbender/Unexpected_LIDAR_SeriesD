#include "io_rgb.h"
#include "dispatcher.h"
#include "UMSeriesD_idf.h"   // <-- use the C header now

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define RGB_CMD_QUEUE_LEN 10
#define RGB_TASK_STACK_SIZE 4096
#define RGB_TASK_PRIORITY 5

// Queue for incoming RGB commands
static QueueHandle_t rgb_cmd_queue = NULL;

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

    // Initialize RGB hardware (C API)
    ums3_set_pixel_brightness(255);   // steady brightness for now
    ums3_set_pixel_color(0, 0, 0);    // LED off initially

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

        // Check for incoming commands (non-blocking)
        if (xQueueReceive(rgb_cmd_queue, &msg, 0) == pdTRUE) {

            // For now: interpret only "set solid color"
            if (msg.data[0] == 0x01 && msg.message_len >= 4) {
                uint8_t r = msg.data[1];
                uint8_t g = msg.data[2];
                uint8_t b = msg.data[3];

                ums3_set_pixel_color(r, g, b);
            }
        }

        // No animation yet — steady color only
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}