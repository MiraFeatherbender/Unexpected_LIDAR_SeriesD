#include "io_log.h"
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"

#define IO_LOG_QUEUE_LEN 16
static QueueHandle_t log_tx_queue = NULL;

void io_log_dispatcher_handler(const dispatcher_msg_t *msg) {
    if (log_tx_queue) {
        xQueueSend(log_tx_queue, msg, portMAX_DELAY);
    }
}

void io_log_init(void) {
    log_tx_queue = xQueueCreate(IO_LOG_QUEUE_LEN, sizeof(dispatcher_msg_t));
    dispatcher_register_handler(TARGET_LOG, io_log_dispatcher_handler);
    xTaskCreate(io_log_event_task, "io_log_event_task", 4096, NULL, 9, NULL);
}

void io_log_event_task(void *arg) {
    dispatcher_msg_t msg = {0};
    while (1) {
        if (xQueueReceive(log_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.source) {
                case SOURCE_LINE_SENSOR:
                case SOURCE_MSC_BUTTON: {
                    size_t hex_len = msg.message_len;
                    if (hex_len > 32) hex_len = 32; // limit log size
                    char hexbuf[32 * 3 + 1] = {0};
                    size_t off = 0;
                    for (size_t i = 0; i < hex_len; ++i) {
                        off += snprintf(hexbuf + off, sizeof(hexbuf) - off, "%02X ", msg.data[i]);
                        if (off >= sizeof(hexbuf)) break;
                    }
                    if (off > 0 && off < sizeof(hexbuf)) {
                        hexbuf[off - 1] = '\0'; // trim trailing space
                    }
                    ESP_LOGI("io_log", "Log message from source %d (hex): %s", msg.source, hexbuf);
                    break;
                }
                default:
                    // null-terminate message data for safety
                    if (msg.message_len >= sizeof(msg.data)) {
                        msg.data[sizeof(msg.data) - 1] = '\0';
                    } else {
                        msg.data[msg.message_len] = '\0';
                    }
                    // Simple log to ESP_LOGI
                    ESP_LOGI("io_log", "Log message from source %d: %s", msg.source, (char*)msg.data);
                    break;
            }
        }
    }
}