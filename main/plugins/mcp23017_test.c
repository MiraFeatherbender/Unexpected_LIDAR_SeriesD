// Simple test harness: send a pooled pointer message to TARGET_MOTOR_DRIVER
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dispatcher_pool.h"
#include "dispatcher.h"
#include "esp_log.h"

static const char *TAG = "mcp_test";

static const uint8_t motor_enable = 1<<4; // bit 4 enables motors

static const uint8_t motor_dir_mask = 0x0F; // lower 4 bits control direction

// Motor direction enum (matches motor driver module)
typedef enum {
    MOTOR_DIR_STOP = 0b00,
    MOTOR_DIR_FORWARD = 0b1001,
    MOTOR_DIR_REVERSE = 0b0110,
    MOTOR_DIR_LEFT_PIVOT = 0b1010,
    MOTOR_DIR_RIGHT_PIVOT = 0b0101,
    MOTOR_DIR_SHORT_BRAKE = 0b1111
} motor_dir_t;

static const char *motor_dir_str(motor_dir_t dir)
{
    switch (dir) {
        case MOTOR_DIR_STOP: return "STOP";
        case MOTOR_DIR_FORWARD: return "FORWARD";
        case MOTOR_DIR_REVERSE: return "REVERSE";
        case MOTOR_DIR_LEFT_PIVOT: return "LEFT_PIVOT";
        case MOTOR_DIR_RIGHT_PIVOT: return "RIGHT_PIVOT";
        case MOTOR_DIR_SHORT_BRAKE: return "SHORT_BRAKE";
        default: return "UNKNOWN";
    }
}

static void mcp23017_test_task(void *arg)
{
    (void)arg;
    // allow system and MCP setup to run
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Motor state machine variable
    motor_dir_t current_dir = MOTOR_DIR_FORWARD;

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = TARGET_MOTOR_DRIVER;

    uint8_t set_data[2] = {motor_enable|motor_dir_mask, 0};

    dispatcher_pool_send_params_t params = {
        .type = DISPATCHER_POOL_CONTROL,
        .source = SOURCE_REST,
        .targets = targets,
        .data = NULL,
        .data_len = 0,
        .context = NULL,
    };

    for (;;) {
        // State machine to cycle forward for 5s, then stop as default with no repeat
        ESP_LOGD(TAG, "Setting motor direction to %s", motor_dir_str(current_dir));
        switch(current_dir) {
            case MOTOR_DIR_FORWARD: 
                set_data[1] = motor_enable | MOTOR_DIR_FORWARD;
                params.data = set_data;
                params.data_len = sizeof(set_data);
                current_dir = MOTOR_DIR_LEFT_PIVOT;
                break;
            case MOTOR_DIR_LEFT_PIVOT:
                set_data[1] = motor_enable | MOTOR_DIR_LEFT_PIVOT;
                params.data = set_data;
                params.data_len = sizeof(set_data);
                current_dir = MOTOR_DIR_REVERSE;
                break;
            case MOTOR_DIR_REVERSE:
                set_data[1] = motor_enable | MOTOR_DIR_REVERSE;
                params.data = set_data;
                params.data_len = sizeof(set_data);
                current_dir = MOTOR_DIR_RIGHT_PIVOT;
                break;
            case MOTOR_DIR_RIGHT_PIVOT:
                set_data[1] = motor_enable | MOTOR_DIR_RIGHT_PIVOT;
                params.data = set_data;
                params.data_len = sizeof(set_data);
                current_dir = MOTOR_DIR_STOP;
                break;
            case MOTOR_DIR_STOP:
                set_data[1] = motor_enable | MOTOR_DIR_STOP;
                params.data = set_data;
                params.data_len = sizeof(set_data);
                current_dir = MOTOR_DIR_STOP; // remain stopped;
                break;
            default:
                break;
        }

        dispatcher_pool_send_ptr_params(&params);

        // Wait 5 seconds before next change
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void mcp23017_test_start(void)
{
    xTaskCreate(mcp23017_test_task, "mcp23017_test", 3072, NULL, 5, NULL);
}
