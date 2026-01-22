#include "mod_line_sensor_window.h"
#include "dispatcher_module.h"
#include "dispatcher.h"
#include <string.h>
#include "esp_log.h"

#define LINE_SENSOR_WINDOW_SIZE 16

static const char *TAG = "line_sensor_window";

static uint8_t line_sensor_window_buf[LINE_SENSOR_WINDOW_SIZE] = {0};
static size_t line_sensor_window_head = 0;
static size_t line_sensor_window_count = 0;

static void line_sensor_window_snapshot(uint8_t *out, size_t *out_len) {
    size_t count = line_sensor_window_count;
    if (count > LINE_SENSOR_WINDOW_SIZE) count = LINE_SENSOR_WINDOW_SIZE;
    size_t start = (line_sensor_window_head + LINE_SENSOR_WINDOW_SIZE - count) % LINE_SENSOR_WINDOW_SIZE;
    size_t first = LINE_SENSOR_WINDOW_SIZE - start;
    if (first > count) first = count;

    if (count == 0) {
        *out_len = 0;
        return;
    }

    memcpy(out, &line_sensor_window_buf[start], first);
    if (count > first) {
        memcpy(out + first, &line_sensor_window_buf[0], count - first);
    }
    *out_len = count;
}

static void line_sensor_window_process_msg(const dispatcher_msg_t *msg) {
    if (!msg || msg->source != SOURCE_LINE_SENSOR || msg->message_len < 1) return;

    uint8_t sample = msg->data[0];
    line_sensor_window_buf[line_sensor_window_head] = sample;
    line_sensor_window_head = (line_sensor_window_head + 1) % LINE_SENSOR_WINDOW_SIZE;
    if (line_sensor_window_count < LINE_SENSOR_WINDOW_SIZE) {
        line_sensor_window_count++;
    }

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = TARGET_SSE_LINE_SENSOR;

    uint8_t snapshot[LINE_SENSOR_WINDOW_SIZE] = {0};
    size_t snapshot_len = 0;
    line_sensor_window_snapshot(snapshot, &snapshot_len);

    if (!dispatcher_pool_send_ptr(DISPATCHER_POOL_STREAMING,
                                  SOURCE_LINE_SENSOR_WINDOW,
                                  targets,
                                  snapshot,
                                  snapshot_len,
                                  NULL)) {
        ESP_LOGW(TAG, "Pool send failed; dropping window msg");
    }
}

static dispatcher_module_t line_sensor_window_mod = {
    .name = "line_sensor_window_task",
    .target = TARGET_LINE_SENSOR_WINDOW,
    .queue_len = 8,
    .stack_size = 4096,
    .task_prio = 9,
    .process_msg = line_sensor_window_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL
};

static void line_sensor_window_dispatcher_handler(const dispatcher_msg_t *msg) {
    dispatcher_module_enqueue(&line_sensor_window_mod, msg);
}

static QueueHandle_t line_sensor_window_ptr_queue = NULL;

static void line_sensor_window_ptr_task(void *arg) {
    (void)arg;
    while (1) {
        pool_msg_t *pmsg = NULL;
        if (xQueueReceive(line_sensor_window_ptr_queue, &pmsg, portMAX_DELAY) == pdTRUE) {
            dispatcher_module_process_ptr_compat(&line_sensor_window_mod, pmsg);
        }
    }
}

void mod_line_sensor_window_init(void) {
    dispatcher_module_start(&line_sensor_window_mod, line_sensor_window_dispatcher_handler);

    line_sensor_window_ptr_queue = dispatcher_ptr_queue_create_register(TARGET_LINE_SENSOR_WINDOW, line_sensor_window_mod.queue_len);
    if (!line_sensor_window_ptr_queue) {
        ESP_LOGE(TAG, "Failed to create pointer queue for line_sensor_window");
        return;
    }

    xTaskCreate(line_sensor_window_ptr_task, "line_sensor_window_ptr_task", line_sensor_window_mod.stack_size, NULL, line_sensor_window_mod.task_prio, NULL);
}
