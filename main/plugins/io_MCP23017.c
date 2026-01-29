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

// Select which MCU GPIO is used for the MCP23017 INT line. Default to GPIO36
#ifndef MCP_INT_GPIO
#define MCP_INT_GPIO GPIO_NUM_2
#endif

// Task-notify based worker handle and minimal diagnostics
static TaskHandle_t s_mcp_gpio_worker_task = NULL;
static volatile uint32_t s_isr_hits = 0;
// Watchdog counters
#ifndef MCP_WATCHDOG_INTERVAL_MS
#define MCP_WATCHDOG_INTERVAL_MS 200
#endif

// INTCAP read retry policy
#ifndef MCP_INTCAP_READ_RETRIES
#define MCP_INTCAP_READ_RETRIES 3
#endif
#ifndef MCP_INTCAP_READ_DELAY_MS
#define MCP_INTCAP_READ_DELAY_MS 1
#endif

// Drain loop backoff and stuck detection
#ifndef MCP_DRAIN_BACKOFF_MS
#define MCP_DRAIN_BACKOFF_MS 5
#endif
#ifndef MCP_INT_STUCK_MS
#define MCP_INT_STUCK_MS 500
#endif
#ifndef MCP_TOGGLE_DELAY_MS
#define MCP_TOGGLE_DELAY_MS 10
#endif

static void IRAM_ATTR mcp_gpio_isr_handler(void *arg)
{
    (void)arg;
    s_isr_hits++;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_mcp_gpio_worker_task) {
        
        vTaskNotifyGiveFromISR(s_mcp_gpio_worker_task, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

static void mcp_gpio_isr_worker(void *arg)
{
    (void)arg;
    for (;;) {
        // Wait for a notification from the ISR (coalesces bursts)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!s_mcp_dev) {
            ESP_LOGW(TAG, "ISR worker: MCP23017 device not ready");
            continue;
        }

        // Read INTCAP once (snapshot when interrupt occurred)
        uint8_t intcap = 0;
        uint8_t gpio_b = 0;
        esp_err_t rc = ESP_ERR_INVALID_RESPONSE;
        while(gpio_get_level(MCP_INT_GPIO) == 0) {

            rc = mcp23017_reg_read8(s_mcp_dev, 0, 0x13, &intcap);
            if (rc == ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(10));
            ESP_LOGI(TAG, "ISR worker: MCP INT still active, reading INTCAP");
            mcp23017_reg_read8(s_mcp_dev, 0, 0x0F, &gpio_b);
        }
        if (rc == ESP_OK) {
            uint8_t intcap_b = intcap;
            ESP_LOGI(TAG, "MCP23017 Port B INTCAP=0x%02X", intcap_b);
        } else {
            mcp23017_read_gpio8(s_mcp_dev, 0, 1, &gpio_b);
            ESP_LOGW(TAG, "ISR worker: INTCAP read failed (0x%X), falling back to GPIO read", rc);
            ESP_LOGI(TAG, "MCP23017 Port B GPIO=0x%02X", gpio_b);
        }

    }
}

static void io_mcp23017_task(void *arg)
{
    (void)arg;
    // Ensure an already-initialized I2C master bus is available (created elsewhere)
    // i2c_master_bus_handle_t bus = NULL;
    // int tries = 0;
    // const int max_tries = 50; // ~10s
    // while (i2c_master_get_bus_handle(I2C_NUM_0, &bus) != ESP_OK) {
    //     if (++tries >= max_tries) break;
    //     ESP_LOGI(TAG, "I2C bus not ready yet, retrying...");
    //     vTaskDelay(pdMS_TO_TICKS(200));
    // }
    // if (!bus) {
    //     ESP_LOGW(TAG, "I2C master bus not initialized; aborting MCP23017 polling task");
    //     vTaskDelete(NULL);
    //     return;
    // }

    // uint8_t addrs[8];
    // int found = 0;
    // ESP_LOGI(TAG, "Scanning I2C bus for MCP23017... (0x20-0x27)");
    // mcp23017_discover_bus(I2C_NUM_0, addrs, &found);
    // if (found == 0) {
    //     ESP_LOGW(TAG, "No MCP23017 devices found on I2C bus");
    //     vTaskDelete(NULL);
    //     return;
    // }

    // ESP_LOGI(TAG, "Found %d MCP23017 device(s), using first at 0x%02X", found, addrs[0]);

    mcp23017_config_t cfg = {0};
    cfg.i2c_port = I2C_NUM_1;
    cfg.sda_gpio = 34;
    cfg.scl_gpio = 36;
    cfg.i2c_freq_hz = 400000;
    cfg.auto_discover = false;
    cfg.addresses[0] = 0x27; // use first address
    cfg.addr_count = 1;
    cfg.bank_16bit = false; // sequential (BANK=0)
    cfg.seqop = false;
    cfg.odr = true;
    cfg.intpol = false;
    cfg.mirror = false;
    cfg.disslw = false;

    mcp23017_handle_t dev;
    if (mcp23017_create(&cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create MCP23017 handle");
        vTaskDelete(NULL);
        return;
    }

    // publish the device handle so ISR worker can query INTF/INTCAP
    s_mcp_dev = dev;

    // Initial safe setup: set both ports A and B as inputs and disable interrupts/pull-ups
    // BANK=0 addresses: IODIRA=0x00, IODIRB=0x01, GPINTENA=0x04, GPINTENB=0x05,
    // GPPUA=0x0C, GPPUB=0x0D, INTCONA=0x08, INTCONB=0x09
    (void)mcp23017_reg_write8(dev, 0, 0x00, 0xFF); // IODIRA = all inputs
    (void)mcp23017_reg_write8(dev, 0, 0x01, 0xFF); // IODIRB = all inputs
    (void)mcp23017_reg_write8(dev, 0, 0x04, 0x00); // GPINTENA = interrupts disabled
    (void)mcp23017_reg_write8(dev, 0, 0x05, 0x00); // GPINTENB = interrupts disabled
    (void)mcp23017_reg_write8(dev, 0, 0x0C, 0x00); // GPPUA = no pullups
    (void)mcp23017_reg_write8(dev, 0, 0x0D, 0x00); // GPPUB = no pullups
    (void)mcp23017_reg_write8(dev, 0, 0x08, 0x00); // INTCONA = compare to previous (clear)
    (void)mcp23017_reg_write8(dev, 0, 0x09, 0x00); // INTCONB = compare to previous (clear)

    // Configure Port B for interrupt testing: enable interrupt-on-change on Port B
    // Keep pull-ups disabled
    (void)mcp23017_reg_write8(dev, 0, 0x0D, 0xFF); // GPPUB = pullups
    (void)mcp23017_reg_write8(dev, 0, 0x05, 0xFF); // GPINTENB = enable interrupts on change (all pins)
    (void)mcp23017_reg_write8(dev, 0, 0x09, 0x00); // INTCONB = 0 -> compare to previous


    // MCP device initialized; ISR worker will report INTF/INTCAP when GPIO ISR fires.
    // Keep this task alive but idle as the ISR-driven worker now handles interrupt reporting.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void io_MCP23017_start(void)
{
    ESP_LOGI(TAG, "io_MCP23017_start() called");
    // Configure MCU GPIO15 for negative-edge interrupt and start logger task
        const gpio_num_t LOG_INT_GPIO = MCP_INT_GPIO;
    gpio_config_t log_io_conf = {
        .pin_bit_mask = (1ULL << LOG_INT_GPIO),
        .mode = GPIO_MODE_INPUT,
        // Enable internal pull-up since MCP23017 INT is typically open-drain
        .pull_up_en = GPIO_PULLDOWN_ENABLE,
        .pull_down_en = GPIO_PULLUP_DISABLE,
        // Use ANYEDGE to catch both polarities during debugging
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&log_io_conf);
        ESP_LOGI(TAG, "Using MCU INT GPIO %d", (int)LOG_INT_GPIO);

    // Install ISR service and add handler (non-fatal if it fails)
    esp_err_t r = gpio_install_isr_service(0);
    ESP_LOGI(TAG, "gpio_install_isr_service() -> 0x%X", r);
    if (r == ESP_OK || r == ESP_ERR_INVALID_STATE) {
        BaseType_t created = xTaskCreate(mcp_gpio_isr_worker, "mcp_gpio_worker", 4096, NULL, 6, &s_mcp_gpio_worker_task);
        ESP_LOGI(TAG, "mcp_gpio_worker task create -> %ld", (long)created);
        if (created == pdPASS) {
            esp_err_t ha = gpio_isr_handler_add(LOG_INT_GPIO, mcp_gpio_isr_handler, (void*)(intptr_t)LOG_INT_GPIO);
            ESP_LOGI(TAG, "gpio_isr_handler_add(%d) -> 0x%X", (int)LOG_INT_GPIO, ha);
            if (ha == ESP_OK) {
                ESP_LOGI(TAG, "GPIO ISR installed on GPIO%d", LOG_INT_GPIO);
            } else {
                ESP_LOGW(TAG, "Failed to add ISR handler for GPIO%d (0x%X)", LOG_INT_GPIO, ha);
            }
        } else {
            ESP_LOGW(TAG, "Failed to create ISR worker task; ISR disabled (create=%ld)", (long)created);
        }
    } else {
        ESP_LOGW(TAG, "gpio_install_isr_service() failed: 0x%X; ISR not enabled", r);
    }

    xTaskCreate(io_mcp23017_task, "io_mcp23017", 4096, NULL, 5, NULL);
    // // Start watchdog to monitor MCP23017 health and help recover from I2C/device issues
    // BaseType_t wd = xTaskCreate(mcp23017_watchdog_task, "mcp23017_wd", 3072, NULL, 5, NULL);
    // ESP_LOGI(TAG, "mcp23017_watchdog task create -> %ld", (long)wd);
}
