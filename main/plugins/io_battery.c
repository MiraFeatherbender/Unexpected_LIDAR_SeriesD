#include "io_battery.h"
#include "dispatcher.h"
#include "rgb_anim.h"
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

// Determine HSV color based on battery state
static hsv_color_t battery_hsv_color(float voltage, bool vbus)
{
    hsv_color_t hsv;
    if (vbus) {
        if (voltage < 4.0f) {
            hsv.h = 170; hsv.s = 255; hsv.v = 255; // Blue (charging)
        } else {
            hsv.h = 0; hsv.s = 0; hsv.v = 0;       // Off (full)
        }
    } else {
        if (voltage >= 3.6f) {
            hsv.h = 85; hsv.s = 255; hsv.v = 255;  // Green (high)
        } else if (voltage >= 3.3f) {
            hsv.h = 30; hsv.s = 255; hsv.v = 255;  // Orange (medium)
        } else {
            hsv.h = 0; hsv.s = 255; hsv.v = 255;   // Red (low)
        }
    }
    return hsv;
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
        // 1. SEND HSV MESSAGE
        // -----------------------------
        hsv_color_t hsv = battery_hsv_color(voltage, vbus);

        dispatcher_msg_t rgb_msg = {0};
        rgb_msg.target = TARGET_RGB;
        rgb_msg.source = SOURCE_UNDEFINED;

        rgb_msg.data[0] = RGB_PLUGIN_FIRE;  // plugin ID
        rgb_msg.data[1] = hsv.h;
        rgb_msg.data[2] = hsv.s;
        rgb_msg.data[3] = hsv.v;
        rgb_msg.data[4] = 25;               // brightness (0–255)
        rgb_msg.message_len = 5;

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