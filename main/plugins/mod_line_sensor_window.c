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

/* Send every Nth snapshot to reduce dispatch pressure. 1 = every sample */
static uint8_t line_sensor_window_snap_div = 1;
static size_t line_sensor_window_snap_counter = 0;

void mod_line_sensor_window_set_snapshot_div(uint8_t div) {
    if (div == 0) div = 1;
    line_sensor_window_snap_div = div;
    ESP_LOGI(TAG, "snapshot divisor set to %u", (unsigned)div);
}

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

    /* Throttle sending: only dispatch every Nth snapshot */
    line_sensor_window_snap_counter = (line_sensor_window_snap_counter + 1) % line_sensor_window_snap_div;
    if (line_sensor_window_snap_counter != 0) {
        return; /* skip dispatch this sample */
    }

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = TARGET_SSE_LINE_SENSOR;

    uint8_t snapshot[LINE_SENSOR_WINDOW_SIZE] = {0};
    size_t snapshot_len = 0;
    line_sensor_window_snapshot(snapshot, &snapshot_len);

    dispatcher_pool_send_params_t params = {
        .type = DISPATCHER_POOL_STREAMING,
        .source = SOURCE_LINE_SENSOR_WINDOW,
        .targets = targets,
        .data = snapshot,
        .data_len = snapshot_len,
        .context = NULL
    };
    if (!dispatcher_pool_send_ptr_params(&params)) {
        ESP_LOGW(TAG, "Pool send failed; dropping window msg");
    }
}

static dispatcher_module_t line_sensor_window_mod = {
    .name = "line_sensor_window_task",
    .target = TARGET_LINE_SENSOR_WINDOW,
    .queue_len = 32,
    .stack_size = 3072,
    .task_prio = 8,
    .process_msg = line_sensor_window_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL
};

void mod_line_sensor_window_init(void) {
    if (dispatcher_module_start(&line_sensor_window_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for line_sensor_window");
        return;
    }
}
