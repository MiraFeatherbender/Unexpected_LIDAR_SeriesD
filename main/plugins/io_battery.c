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


// Central struct for RGB plugin, HSV, and brightness per tier
typedef struct {
    float min_voltage;
    bool vbus_required; // true: charging, false: discharging
    uint8_t plugin;
    uint8_t h, s, v, brightness;
} battery_rgb_tier_t;

static const battery_rgb_tier_t battery_rgb_tiers[] = {
    // Charging tiers
    
    { 0.0f,  true, RGB_PLUGIN_WATER,     130,   0,   0,   40 }, // Charging, low voltage
    { 3.7f,  true, RGB_PLUGIN_WATER,     130,   0,   0,   40 }, // Charging, mid voltage
    { 4.0f,  true, RGB_PLUGIN_AURORA,     84,   0,   0,   30 }, // Charging, full
    // Discharging tiers
    { 0.0f, false, RGB_PLUGIN_HEARTBEAT,   0, 255, 255,  255 }, // Discharging, low
    { 3.3f, false, RGB_PLUGIN_FIRE,       15, 255, 215,  179 }, // Discharging, medium
    { 3.6f, false, RGB_PLUGIN_BREATHE,    88, 255, 220,  255 }, // Discharging, high
};

#define NUM_TIERS (sizeof(battery_rgb_tiers)/sizeof(battery_rgb_tiers[0]))

// Select the appropriate RGB tier for the current voltage and VBUS state
static const battery_rgb_tier_t* battery_get_rgb_tier(float voltage, bool vbus) {
    const battery_rgb_tier_t* selected = NULL;
    for (size_t i = 0; i < NUM_TIERS; ++i) {
        if (battery_rgb_tiers[i].vbus_required == vbus && voltage >= battery_rgb_tiers[i].min_voltage) {
            selected = &battery_rgb_tiers[i];
        }
    }
    return selected;
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
        // 1. SEND RGB TIER MESSAGE
        // -----------------------------
        dispatcher_msg_t rgb_msg = {0};
        memset(rgb_msg.targets, TARGET_MAX, sizeof(rgb_msg.targets));
        rgb_msg.targets[0] = TARGET_RGB;
        rgb_msg.source = SOURCE_BATTERY;

        const battery_rgb_tier_t* tier = battery_get_rgb_tier(voltage, vbus);
        if (tier) {
            rgb_msg.data[0] = tier->plugin;
            rgb_msg.data[1] = tier->h;
            rgb_msg.data[2] = tier->s;
            rgb_msg.data[3] = tier->v;
            rgb_msg.data[4] = tier->brightness;
        } else {
            // fallback/default values
            rgb_msg.data[0] = RGB_PLUGIN_AURORA;
            rgb_msg.data[1] = 84;
            rgb_msg.data[2] = 0;
            rgb_msg.data[3] = 0;
            rgb_msg.data[4] = 30;
        }
        rgb_msg.message_len = 5;

        dispatcher_send(&rgb_msg);

        // -----------------------------
        // 2. SEND USB TEXT MESSAGE
        // -----------------------------
        dispatcher_msg_t usb_msg = {0};
        memset(usb_msg.targets, TARGET_MAX, sizeof(usb_msg.targets));
        usb_msg.targets[0] = TARGET_USB_CDC;
        usb_msg.source = SOURCE_BATTERY;

        usb_msg.message_len = snprintf(
            (char *)usb_msg.data,
            BUF_SIZE,
            "VBUS=%d %s Voltage=%.2fV Percent=%d%%\r\n",
            vbus ? 1 : 0,
            state,
            voltage,
            percent
        );

        //dispatcher_send(&usb_msg);

        // -----------------------------
        // Wait for next cycle
        // -----------------------------
        vTaskDelay(pdMS_TO_TICKS(BATTERY_CHECK_INTERVAL_MS));
    }
}