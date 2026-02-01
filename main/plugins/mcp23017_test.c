// Simple test harness: send a pooled pointer message to TARGET_MOTOR_DRIVER
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dispatcher_pool.h"
#include "dispatcher.h"
#include "esp_log.h"

static const char *TAG = "mcp_test";

static void mcp23017_test_task(void *arg)
{
    (void)arg;
    // allow system and MCP setup to run
    vTaskDelay(pdMS_TO_TICKS(3000));

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = TARGET_MOTOR_DRIVER;

    for (;;) {
        // Set bit 0
        uint8_t set_data[2] = { 0x01, 0x01 };
        pool_msg_t *p1 = dispatcher_pool_send_ptr(DISPATCHER_POOL_CONTROL, SOURCE_REST, targets, set_data, sizeof(set_data), NULL);


        vTaskDelay(pdMS_TO_TICKS(2000));

        // Clear bit 0
        uint8_t clr_data[2] = { 0x01, 0x00 };
        pool_msg_t *p2 = dispatcher_pool_send_ptr(DISPATCHER_POOL_CONTROL, SOURCE_REST, targets, clr_data, sizeof(clr_data), NULL);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void mcp23017_test_start(void)
{
    xTaskCreate(mcp23017_test_task, "mcp23017_test", 3072, NULL, 5, NULL);
}
