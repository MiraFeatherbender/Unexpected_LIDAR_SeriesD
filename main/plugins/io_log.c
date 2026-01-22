#include "io_log.h"
#include "dispatcher_module.h"
#include "string.h"
#include "esp_log.h"

static void io_log_process_msg(const dispatcher_msg_t *msg) {
    if (!msg) return;
    switch (msg->source) {
        case SOURCE_LINE_SENSOR_WINDOW:
        case SOURCE_LINE_SENSOR:
        case SOURCE_MSC_BUTTON: {
            size_t hex_len = msg->message_len;
            if (hex_len > 32) hex_len = 32; // limit log size
            char hexbuf[32 * 3 + 1] = {0};
            size_t off = 0;
            for (size_t i = 0; i < hex_len; ++i) {
                off += snprintf(hexbuf + off, sizeof(hexbuf) - off, "%02X ", msg->data[i]);
                if (off >= sizeof(hexbuf)) break;
            }
            if (off > 0 && off < sizeof(hexbuf)) {
                hexbuf[off - 1] = '\0'; // trim trailing space
            }
            ESP_LOGI("io_log", "Log message from source %d (hex): %s", msg->source, hexbuf);
            break; 
        }
        default:
            // null-terminate message data for safety
            if (msg->message_len >= sizeof(msg->data)) {
                ((uint8_t *)msg->data)[sizeof(msg->data) - 1] = '\0';
            } else {
                ((uint8_t *)msg->data)[msg->message_len] = '\0';
            }
            // Simple log to ESP_LOGI
            ESP_LOGI("io_log", "Log message from source %d: %s", msg->source, (char*)msg->data);
            break;
    }
}

static dispatcher_module_t io_log_mod = {
    .name = "io_log_task",
    .target = TARGET_LOG,
    .queue_len = 16,
    .stack_size = 4096,
    .task_prio = 9,
    .process_msg = io_log_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL
};

static void io_log_dispatcher_handler(const dispatcher_msg_t *msg) {
    dispatcher_module_enqueue(&io_log_mod, msg);
}

static QueueHandle_t io_log_ptr_queue = NULL;

static void io_log_ptr_task(void *arg) {
    (void)arg;
    while (1) {
        pool_msg_t *pmsg = NULL;
        if (xQueueReceive(io_log_ptr_queue, &pmsg, portMAX_DELAY) == pdTRUE) {
            dispatcher_module_process_ptr_compat(&io_log_mod, pmsg);
        }
    }
}

void io_log_init(void) {
    dispatcher_module_start(&io_log_mod, io_log_dispatcher_handler);

    io_log_ptr_queue = dispatcher_ptr_queue_create_register(TARGET_LOG, io_log_mod.queue_len);
    if (!io_log_ptr_queue) {
        ESP_LOGE("io_log", "Failed to create pointer queue for io_log");
        return;
    }

    xTaskCreate(io_log_ptr_task, "io_log_ptr_task", io_log_mod.stack_size, NULL, io_log_mod.task_prio, NULL);
}