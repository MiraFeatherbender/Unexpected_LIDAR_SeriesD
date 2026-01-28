#include "io_log.h"
#include "dispatcher_module.h"
#include "string.h"
#include "esp_log.h"

static void io_log_process_msg(const dispatcher_msg_t *msg) {
    if (!msg) return;
    switch (msg->source) {
        case SOURCE_ULTRASONIC:
            // Log ultrasonic distance messages as 16-bit decimal
            size_t dec_len = msg->message_len;
            if (dec_len > 32) dec_len = 32; // limit log size
            char decbuf[32 * 3 + 1] = {0};
            uint16_t val = (uint16_t)msg->data[0] | ((uint16_t)msg->data[1] << 8);
            snprintf(decbuf, sizeof(decbuf), "%u ", val);
            
            ESP_LOGI("io_log", "Log message from ultrasonic sensor: %s mm", decbuf);
            break;
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

void io_log_init(void) {
    if (dispatcher_module_start(&io_log_mod) != pdTRUE) {
        ESP_LOGE("io_log", "Failed to start dispatcher module for io_log");
        return;
    }
}