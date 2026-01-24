#include "dispatcher_pool_test.h"

#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "dispatcher_module.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define POOL_TEST_QUEUE_LEN 4
#define POOL_TEST_TASK_STACK 3072
#define POOL_TEST_TASK_PRIO 8
#define POOL_TEST_SEND_MS 1000

static const char *TAG = "dispatcher_pool_test";

static void pool_test_process_msg(const dispatcher_msg_t *msg);

static dispatcher_module_t pool_test_mod = {
    .name = "pool_test",
    .target = TARGET_POOL_TEST,
    .queue_len = POOL_TEST_QUEUE_LEN,
    .stack_size = POOL_TEST_TASK_STACK,
    .task_prio = POOL_TEST_TASK_PRIO,
    .process_msg = pool_test_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL
};


static void pool_test_tx_task(void *arg) {
    (void)arg;
    uint8_t counter = 0;
    while (1) {
        pool_msg_t *pmsg = dispatcher_pool_try_alloc(DISPATCHER_POOL_STREAMING);
        if (pmsg) {
            dispatcher_msg_ptr_t *msg = dispatcher_pool_get_msg(pmsg);
            if (msg && msg->data) {
                msg->source = SOURCE_POOL_TEST;
                dispatcher_fill_targets(msg->targets);
                msg->targets[0] = TARGET_POOL_TEST;
                msg->message_len = 8;
                for (size_t i = 0; i < msg->message_len; ++i) {
                    msg->data[i] = (uint8_t)(counter + i);
                }
                dispatcher_broadcast_ptr(pmsg, msg->targets);
                counter++;
            } else {
                dispatcher_pool_msg_unref(pmsg);
            }
        } else {
            ESP_LOGW(TAG, "TX alloc failed");
        }
        vTaskDelay(pdMS_TO_TICKS(POOL_TEST_SEND_MS));
    }
}

static void pool_test_process_msg(const dispatcher_msg_t *msg) {
    if (!msg) return;
    if (msg->message_len > 0) {
        size_t len = msg->message_len;
        if (len > 4) len = 4;
        ESP_LOGI(TAG, "RX len=%u first=%02X %02X %02X %02X",
                 (unsigned)msg->message_len,
                 msg->data[0], msg->data[1], msg->data[2], msg->data[3]);
    } else {
        ESP_LOGW(TAG, "RX msg missing payload");
    }
}

void dispatcher_pool_test_init(void) {
#ifdef CONFIG_DISPATCHER_POOL_TEST
    if (dispatcher_module_start(&pool_test_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for pool_test");
        return;
    }

    // Start the TX task
    xTaskCreate(pool_test_tx_task, "pool_test_tx", POOL_TEST_TASK_STACK, NULL, POOL_TEST_TASK_PRIO, NULL);
#else
    (void)pool_test_mod;
#endif
}
