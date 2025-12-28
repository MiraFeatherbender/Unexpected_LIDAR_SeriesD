#include "io_battery.h"
#include "dispatcher.h"
#include "UMSeriesD_idf.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

#define BATTERY_TASK_STACK_SIZE 4096
#define BATTERY_TASK_PRIORITY   5
#define BATTERY_CHECK_INTERVAL_MS 3000

// Forward declaration
static void io_battery_task(void *arg);

// Convert voltage to approximate percent (simple linear mapping)
static int battery_percent_from_voltage(float v)
{
    // Clamp voltage range 3.30 → 4.20
    if (v >= 4.20f) return 100;
    if (v <= 3.30f) return 0;

    float pct = (v - 3.30f) / (4.20f - 3.30f);
    return (int)(pct * 100.0f);
}

// Determine 32-bit packed color based on battery state
static uint32_t battery_color(float voltage, bool vbus)
{
    if (vbus) {
        if (voltage < 4.0f) {
            return 0x0000FF; // Blue (charging)
        } else {
            return 0x000000; // Off (full)
        }
    } else {
        if (voltage >= 3.6f) {
            return 0x00FF00; // Green (high)
        } else if (voltage >= 3.3f) {
            return 0xFF8800; // Orange (medium)
        } else {
            return 0xFF0000; // Red (low)
        }
    }
}

void io_battery_init(void)
{
    // Initialize MAX17048 fuel gauge
    ums3_fg_setup();

    // Create task
    xTaskCreate(io_battery_task,
                "io_battery_task",
                BATTERY_TASK_STACK_SIZE,
                NULL,
                BATTERY_TASK_PRIORITY,
                NULL);
}

static void io_battery_task(void *arg)
{
    (void)arg;

    while (1) {

        float voltage = ums3_get_battery_voltage();
        bool vbus = ums3_get_vbus_present();
        int percent = battery_percent_from_voltage(voltage);

        // Determine state string
        const char *state;
        if (vbus) {
            state = (voltage < 4.0f) ? "CHARGING" : "FULL";
        } else {
            state = "DISCHARGING";
        }

        // -----------------------------
        // 1. SEND RGB MESSAGE
        // -----------------------------
        uint32_t color = battery_color(voltage, vbus);

        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8)  & 0xFF;
        uint8_t b = (color >> 0)  & 0xFF;

        dispatcher_msg_t rgb_msg = {0};
        rgb_msg.target = TARGET_RGB;
        rgb_msg.source = SOURCE_UNDEFINED;

        rgb_msg.data[0] = 0x01;  // RGB_CMD_SET_COLOR (your existing opcode)
        rgb_msg.data[1] = r;
        rgb_msg.data[2] = g;
        rgb_msg.data[3] = b;
        rgb_msg.message_len = 4;

        dispatcher_send(&rgb_msg);

        // -----------------------------
        // 2. SEND USB TEXT MESSAGE
        // -----------------------------
        dispatcher_msg_t usb_msg = {0};
        usb_msg.target = TARGET_USB;
        usb_msg.source = SOURCE_UNDEFINED;

        usb_msg.message_len = snprintf(
            (char *)usb_msg.data,
            BUF_SIZE,
            "VBUS=%d %s Voltage=%.2fV Percent=%d%%\r\n",
            vbus ? 1 : 0,
            state,
            voltage,
            percent
        );

        dispatcher_send(&usb_msg);

        // -----------------------------
        // Wait for next cycle
        // -----------------------------
        vTaskDelay(pdMS_TO_TICKS(BATTERY_CHECK_INTERVAL_MS));
    }
}