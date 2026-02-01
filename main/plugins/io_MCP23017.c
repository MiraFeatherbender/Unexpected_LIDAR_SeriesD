// Dispatcher-based MCP23017 module
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "dispatcher_module.h"
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "MCP23017.h"
#include "driver/gpio.h"

static const char *TAG = "io_MCP23017";

// Shared handle for the discovered MCP23017 so ISR worker and msg handler can access it
static mcp23017_handle_t s_mcp_dev = NULL;
static mcp23017_attached_devices_t s_mcp_devices = {0};

// Select which MCU GPIO is used for the MCP23017 INT line (kept from previous design)
#define MCP_INT_GPIOB GPIO_NUM_2

// ISR worker task handle
static TaskHandle_t s_mcp_gpio_worker_task = NULL;

// Forward declarations
static void io_mcp23017_process_msg(const dispatcher_msg_t *msg);
static void mcp_gpio_isr_worker(void *arg);
void io_MCP23017_init(void);

// Dispatcher module instance: this module listens for TARGET_MOTOR_DRIVER messages
static dispatcher_module_t io_mcp23017_mod = {
    .name = "io_MCP23017",
    .target = TARGET_MOTOR_DRIVER,
    .queue_len = 16,
    .stack_size = 4096,
    .task_prio = 5,
    .process_msg = io_mcp23017_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL,
    .next_step = 0,
    .last_queue_warn = 0
};

// ISR worker: waits for notifications from ISR and dumps INTF/INTCAP for diagnostics
static void mcp_gpio_isr_worker(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!s_mcp_dev) {
            ESP_LOGW(TAG, "ISR worker: MCP23017 device not ready");
            continue;
        }

        uint8_t buf[4] = {0};
        esp_err_t rc = mcp23017_reg_read_block(s_mcp_dev, 0, MCP_REG_INTFA, buf, sizeof(buf));
        if (rc != ESP_OK) {
            ESP_LOGW(TAG, "ISR worker: block read INTFA..INTCAPB failed (0x%X)", rc);
            continue;
        }
        uint8_t intf_a = buf[0];
        uint8_t intf_b = buf[1];
        uint8_t intcap_a = buf[2];
        uint8_t intcap_b = buf[3];

        if (intf_a == 0 && intf_b == 0) {
            ESP_LOGW(TAG, "ISR worker: spurious INT - no INTF flags set");
            continue;
        }

        if (intf_a) ESP_LOGI(TAG, "MCP23017 Port A INTF=0x%02X INTCAP=0x%02X", intf_a, intcap_a);
        if (intf_b) ESP_LOGI(TAG, "MCP23017 Port B INTF=0x%02X INTCAP=0x%02X", intf_b, intcap_b);
    }
}

// Background task: perform auto-setup and port configuration so initialization
// doesn't block system bring-up.
static void io_mcp23017_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "io_mcp23017_task: starting auto-setup");

    esp_err_t rc = mcp23017_auto_setup(&s_mcp_devices, true, NULL);
    if (rc != ESP_OK || s_mcp_devices.handle == NULL) {
        ESP_LOGE(TAG, "io_mcp23017_task: Failed to auto-setup MCP23017: 0x%X", rc);
        vTaskDelete(NULL);
        return;
    }
    s_mcp_dev = s_mcp_devices.handle;

    // Configure Port B as inputs with pullups and interrupt on any edge
    mcp23017_pin_cfg_t cfg = {
        .port = MCP_PORT_B,
        .mask = MCP_PORT_ALL,
        .pin_mode = MCP_PIN_INPUT,
        .pullup = MCP_PULLUP_ENABLE,
        .int_mode = MCP_INT_ANYEDGE,
        .int_polarity = MCP_INT_OPENDRAIN,
        .initial_level = 0x00,
        .flags = MCP_CFG_BATCH_WRITE,
    };
    (void)mcp23017_config_port(s_mcp_dev, 0, &cfg);

    // Configure Port A as outputs for motor control
    cfg.port = MCP_PORT_A;
    cfg.mask = MCP_PORT_ALL;
    cfg.pin_mode = MCP_PIN_OUTPUT;
    cfg.pullup = MCP_PULLUP_DISABLE;
    cfg.int_mode = MCP_INT_NONE;
    cfg.int_polarity = MCP_INT_OPENDRAIN;
    cfg.initial_level = 0x00;
    cfg.flags = MCP_CFG_BATCH_WRITE;
    (void)mcp23017_config_port(s_mcp_dev, 0, &cfg);

    // Enable mirrored INT output
    (void)mcp23017_set_int_mirror(s_mcp_dev, 0, true);

    ESP_LOGI(TAG, "io_mcp23017_task: setup complete");

    // Terminate this one-shot task
    vTaskDelete(NULL);
}

// Message format consumed by TARGET_MOTOR_DRIVER (pooled pointer messages):
// data[0] = mask (bits to affect on the port)
// data[1] = value (bits to set where mask==1)
static void io_mcp23017_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg) return;
    if (!s_mcp_dev) {
        ESP_LOGW(TAG, "process_msg: MCP device not initialized");
        return;
    }

    if (msg->message_len < 2) {
        ESP_LOGW(TAG, "process_msg: message too short (%u)", (unsigned)msg->message_len);
        return;
    }

    uint8_t mask = (uint8_t)msg->data[0];
    uint8_t value = (uint8_t)msg->data[1];

    // For this module we hard-code device index 0 and Port A for motor driver outputs
    esp_err_t rc = mcp23017_port_masked_write(s_mcp_dev, 0, MCP_PORT_A, mask, value);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "mcp masked write failed: 0x%X (mask=0x%02X val=0x%02X)", rc, mask, value);
    } else {
        ESP_LOGD(TAG, "mcp masked write ok (mask=0x%02X val=0x%02X)", mask, value);
    }
}

// Initialize the module: auto-setup MCP devices, configure ports, register ISR worker, start dispatcher module
void io_MCP23017_init(void)
{
    ESP_LOGI(TAG, "io_MCP23017_init() starting auto-setup");

    esp_err_t rc = mcp23017_auto_setup(&s_mcp_devices, true, NULL);
    if (rc != ESP_OK || s_mcp_devices.handle == NULL) {
        ESP_LOGE(TAG, "Failed to auto-setup MCP23017: 0x%X", rc);
        return;
    }
    s_mcp_dev = s_mcp_devices.handle;

    // Configure Port B as inputs with pullups and interrupt on any edge
    mcp23017_pin_cfg_t cfg = {
        .port = MCP_PORT_B,
        .mask = MCP_PORT_ALL,
        .pin_mode = MCP_PIN_INPUT,
        .pullup = MCP_PULLUP_ENABLE,
        .int_mode = MCP_INT_ANYEDGE,
        .int_polarity = MCP_INT_OPENDRAIN,
        .initial_level = 0x00,
        .flags = MCP_CFG_BATCH_WRITE,
    };
    (void)mcp23017_config_port(s_mcp_dev, 0, &cfg);

    // Configure Port A as outputs for motor control
    cfg.port = MCP_PORT_A;
    cfg.mask = MCP_PORT_ALL;
    cfg.pin_mode = MCP_PIN_OUTPUT;
    cfg.pullup = MCP_PULLUP_DISABLE;
    cfg.int_mode = MCP_INT_NONE;
    cfg.int_polarity = MCP_INT_OPENDRAIN;
    cfg.initial_level = 0x00;
    cfg.flags = MCP_CFG_BATCH_WRITE;
    (void)mcp23017_config_port(s_mcp_dev, 0, &cfg);

    // Enable mirrored INT output
    (void)mcp23017_set_int_mirror(s_mcp_dev, 0, true);

    // Create ISR worker task and register with MCP ISR helper (worker may run before device attached)
    BaseType_t created = xTaskCreate(mcp_gpio_isr_worker, "mcp_gpio_worker", 4096, NULL, 6, &s_mcp_gpio_worker_task);
    if (created == pdPASS) {
        mcp23017_isr_cfg_t isr_cfg = {
            .int_gpio = MCP_INT_GPIOB,
            .pull_mode = GPIO_PULLUP_ONLY,
            .intr_type = GPIO_INTR_NEGEDGE,
            .install_isr_service = MCP_ISR_SERVICE_YES,
        };
        (void)mcp23017_isr_register(&isr_cfg, s_mcp_gpio_worker_task);
    } else {
        ESP_LOGW(TAG, "Failed to create ISR worker task; ISR disabled");
    }

    // Start dispatcher module (creates pointer queue and processing task)
    if (dispatcher_module_start(&io_mcp23017_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for io_MCP23017");
        return;
    }

    // Launch background setup task so init does not block other subsystems
    if (xTaskCreate(io_mcp23017_task, "io_mcp23017_setup", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Failed to create MCP23017 setup task");
    }

    ESP_LOGI(TAG, "io_MCP23017_init() started (setup running in background)");
}
