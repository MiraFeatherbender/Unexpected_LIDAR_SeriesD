// Simple plugin: discover MCP23017 and poll Port B every 2s (no interrupts, no blink)
#include <stdio.h>
#include <string.h>
#include "driver/i2c_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "MCP23017.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

static const char *TAG = "io_MCP23017";

// Shared handle for the discovered MCP23017 so ISR worker can access it
static mcp23017_handle_t s_mcp_dev = NULL;
static mcp23017_attached_devices_t s_mcp_devices = {0};

// Select which MCU GPIO is used for the MCP23017 INT line.
#define MCP_INT_GPIOA GPIO_NUM_1
#define MCP_INT_GPIOB GPIO_NUM_2

// Task-notify based worker handle and minimal diagnostics
static TaskHandle_t s_mcp_gpio_worker_task = NULL;

static void mcp_gpio_isr_worker(void *arg)
{
    (void)arg;
    for (;;) {
        // Wait for notification from ISR (coalesces bursts)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!s_mcp_dev) {
            ESP_LOGW(TAG, "ISR worker: MCP23017 device not ready");
            continue;
        }

        // Perform an atomic block read of INTFA..INTCAPB and use the
        // returned INTF values as the probe to determine which port(s)
        // triggered. This reduces I2C transactions compared to pre-reading
        // each INT flag separately.
        uint8_t buf[4] = {0};
        esp_err_t rc = mcp23017_reg_read_block(s_mcp_dev, 0, MCP_REG_INTFA, buf, sizeof(buf));
        if (rc != ESP_OK) {
            ESP_LOGW(TAG, "ISR worker: block read INTFA..INTCAPB failed (0x%X)", rc);
            continue;
        }
        // buf: [INTFA, INTFB, INTCAPA, INTCAPB]
        uint8_t _intf_a = buf[0];
        uint8_t _intf_b = buf[1];
        uint8_t _intcap_a = buf[2];
        uint8_t _intcap_b = buf[3];

        if (_intf_a == 0 && _intf_b == 0) {
            ESP_LOGW(TAG, "ISR worker: spurious INT - no INTF flags set (after block read)");
            continue;
        }

        if (_intf_a) {
            ESP_LOGI(TAG, "MCP23017 Port A INTF=0x%02X INTCAP=0x%02X", _intf_a, _intcap_a);
        }
        if (_intf_b) {
            ESP_LOGI(TAG, "MCP23017 Port B INTF=0x%02X INTCAP=0x%02X", _intf_b, _intcap_b);
        }
    }
}

static void io_mcp23017_task(void *arg)
{
    (void)arg;

    // use auto-setup to probe existing I2C bus and attach devices
    // request that the component apply datasheet defaults to discovered devices
    // allow retries and delay similar to previous behavior in this plugin
    esp_err_t rc = mcp23017_auto_setup(&s_mcp_devices, true, NULL);
    if (rc != ESP_OK || s_mcp_devices.handle == NULL) {
        ESP_LOGE(TAG, "Failed to auto-setup MCP23017: 0x%X", rc);
        vTaskDelete(NULL);
        return;
    }
    s_mcp_dev = s_mcp_devices.handle;

    // The auto_setup applied datasheet defaults; now configure Port B with a batched RMW
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

    // The auto_setup applied datasheet defaults; now configure Port A with a batched RMW
    cfg.port = MCP_PORT_A;
    cfg.mask = 0xFF;
    cfg.pin_mode = MCP_PIN_OUTPUT;
    cfg.pullup = MCP_PULLUP_DISABLE;
    cfg.int_mode = MCP_INT_NONE;
    cfg.int_polarity = MCP_INT_OPENDRAIN;
    cfg.initial_level = 0x00;
    cfg.flags = MCP_CFG_BATCH_WRITE;

    (void)mcp23017_config_port(s_mcp_dev, 0, &cfg);
    
    // Enable mirrored INT output so a single MCU GPIO sees interrupts from both ports
    esp_err_t mirror_rc = mcp23017_set_int_mirror(s_mcp_dev, 0, true);
    if (mirror_rc == ESP_OK) {
        ESP_LOGI(TAG, "MCP23017 INT mirror enabled");
    } else {
        ESP_LOGW(TAG, "Failed to enable MCP23017 INT mirror: 0x%X", mirror_rc);
    }

    // MCP device initialized; ISR worker will report INTF/INTCAP when GPIO ISR fires.
    // Keep this task alive but idle as the ISR-driven worker now handles interrupt reporting.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void io_MCP23017_start(void)
{
    ESP_LOGI(TAG, "io_MCP23017_start() called");

    // Create a single worker and register it for the mirrored INT line on MCP_INT_GPIOB
    BaseType_t created = xTaskCreate(mcp_gpio_isr_worker, "mcp_gpio_worker", 4096, NULL, 6, &s_mcp_gpio_worker_task);
    ESP_LOGI(TAG, "mcp_gpio_worker task create -> %ld", (long)created);
    if (created == pdPASS) {
        mcp23017_isr_cfg_t isr_cfg = {
            .int_gpio = MCP_INT_GPIOB, // use pin assigned to B (mirror mode)
            .pull_mode = GPIO_PULLUP_ONLY,
            .intr_type = GPIO_INTR_NEGEDGE,
            .install_isr_service = MCP_ISR_SERVICE_YES,
        };
        esp_err_t reg = mcp23017_isr_register(&isr_cfg, s_mcp_gpio_worker_task);
        ESP_LOGI(TAG, "mcp23017_isr_register() -> 0x%X", reg);
        if (reg != ESP_OK) {
            ESP_LOGW(TAG, "mcp23017_isr_register() failed: 0x%X", reg);
        }
    } else {
        ESP_LOGW(TAG, "Failed to create ISR worker task; ISR disabled (create=%ld)", (long)created);
    }
    xTaskCreate(io_mcp23017_task, "io_mcp23017", 4096, NULL, 5, NULL);
}
