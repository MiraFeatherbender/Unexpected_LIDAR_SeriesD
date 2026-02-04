#include "io_battery.h"
#include "dispatcher.h"
#include "dispatcher_module.h"
#include "dispatcher_pool.h"
#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include "battery_json.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define BATTERY_CHECK_INTERVAL_MS 500
static const char *TAG = "io_battery";


// Forward declarations
static void battery_process_msg(const dispatcher_msg_t *msg);
static void battery_step_frame(void);

// Module instance (file-scope) used by pointer task
static dispatcher_module_t battery_mod = {
    .name = "io_battery",
    .target = TARGET_BATTERY,
    .queue_len = 4,
    .stack_size = 8192,
    .task_prio = 5,
    .process_msg = battery_process_msg,
    .step_frame = battery_step_frame,
    .step_ms = BATTERY_CHECK_INTERVAL_MS,
    .queue = NULL
};


static bool battery_paused = false;

static void battery_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg) return;
    if (msg->message_len < 1) {
        ESP_LOGW(TAG, "battery_process_msg: empty message");
        return;
    }

    uint8_t cmd = msg->data[0];
    switch ((battery_cmd_t)cmd) {
        case BATTERY_CMD_NONE:
            break;
        case BATTERY_CMD_PAUSE: {
            if (!battery_paused) {
                battery_paused = true;
                ESP_LOGI(TAG, "Received BATTERY_CMD_PAUSE: pausing battery step_frame");
                dispatch_target_t targets[TARGET_MAX];
                dispatcher_fill_targets(targets);
                targets[0] = TARGET_LOG;
                const char *txt = "Battery updates paused";
                dispatcher_pool_send_params_t params = {
                    .type = DISPATCHER_POOL_CONTROL,
                    .source = SOURCE_BATTERY,
                    .targets = targets,
                    .data = (const uint8_t *)txt,
                    .data_len = strlen(txt),
                    .context = NULL
                };
                dispatcher_pool_send_ptr_params(&params);
            }
            break;
        }
        case BATTERY_CMD_RESUME: {
            if (battery_paused) {
                battery_paused = false;
                ESP_LOGI(TAG, "Received BATTERY_CMD_RESUME: resuming battery step_frame");
                dispatch_target_t targets[TARGET_MAX];
                dispatcher_fill_targets(targets);
                targets[0] = TARGET_LOG;
                const char *txt = "Battery updates resumed";
                dispatcher_pool_send_params_t params = {
                    .type = DISPATCHER_POOL_CONTROL,
                    .source = SOURCE_BATTERY,
                    .targets = targets,
                    .data = (const uint8_t *)txt,
                    .data_len = strlen(txt),
                    .context = NULL
                };
                dispatcher_pool_send_ptr_params(&params);
            }
            break;
        }
        case BATTERY_CMD_OVERRIDE_PLUGIN: {
            int plugin = (msg->message_len > 1) ? (int)msg->data[1] : -1;
            ESP_LOGI(TAG, "Received BATTERY_CMD_OVERRIDE_PLUGIN plugin=%d", plugin);
            // Future: set override plugin state
            break;
        }
        case BATTERY_CMD_CLEAR_OVERRIDE:
            ESP_LOGI(TAG, "Received BATTERY_CMD_CLEAR_OVERRIDE");
            // Future: clear override state
            break;
        case BATTERY_CMD_SET_INTERVAL_MS: {
            if (msg->message_len >= 5) {
                uint32_t v = 0;
                // little-endian decode from data[1..4]
                v = (uint32_t)msg->data[1] | ((uint32_t)msg->data[2] << 8) | ((uint32_t)msg->data[3] << 16) | ((uint32_t)msg->data[4] << 24);
                ESP_LOGI(TAG, "Received BATTERY_CMD_SET_INTERVAL_MS=%u ms", (unsigned)v);
                // Apply to module timing
                battery_mod.step_ms = v;
            } else {
                ESP_LOGW(TAG, "BATTERY_CMD_SET_INTERVAL_MS payload too short");
            }
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown battery cmd: %d", cmd);
            break;
    }
}
// Convert voltage to approximate percent (simple linear mapping)
static int battery_percent_from_voltage(float v)
{
    // Clamp voltage range 3.30 → 4.20
    if (v >= 4.20f) return 100;
    if (v <= 3.30f) return 0;

    float pct = (v - 3.30f) / (4.20f - 3.30f);
    return (int)(pct * 100.0f);
}

static const battery_rgb_tier_t battery_rgb_tiers[] = {
    // Charging tiers
    { 0.0f,  true, RGB_PLUGIN_WATER,     130,   0,   0,   40 },
    { 3.7f,  true, RGB_PLUGIN_WATER,     130,   0,   0,   40 },
    { 4.0f,  true, RGB_PLUGIN_AURORA,     84,   0,   0,   30 },
    // Discharging tiers
    { 0.0f, false, RGB_PLUGIN_HEARTBEAT,   0, 255, 255,  255 },
    { 3.3f, false, RGB_PLUGIN_FIRE,       15, 255, 215,  179 },
    { 3.6f, false, RGB_PLUGIN_BREATHE,    88, 255, 220,  255 },
};

#define NUM_TIERS (sizeof(battery_rgb_tiers)/sizeof(battery_rgb_tiers[0]))

// Select the appropriate RGB tier for the current voltage and VBUS state
static const battery_rgb_tier_t* battery_get_rgb_tier(float voltage, bool vbus) {
    size_t num_json_tiers = 0;
    const battery_rgb_tier_t *json_tiers = battery_json_get_tiers(&num_json_tiers);
    const battery_rgb_tier_t *selected = NULL;
    if (json_tiers && num_json_tiers > 0) {
        for (size_t i = 0; i < num_json_tiers; ++i) {
            if (json_tiers[i].vbus_required == vbus && voltage >= json_tiers[i].min_voltage) {
                selected = &json_tiers[i];
            }
        }
    } else {
        for (size_t i = 0; i < NUM_TIERS; ++i) {
            if (battery_rgb_tiers[i].vbus_required == vbus && voltage >= battery_rgb_tiers[i].min_voltage) {
                selected = &battery_rgb_tiers[i];
            }
        }
    }
    return selected;
}

void io_battery_init(void)
{
    // Initialize MAX17048 fuel gauge
    ums3_fg_setup();

    // Try to load battery tiers from JSON at startup
    battery_json_reload();

    if (dispatcher_module_start(&battery_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for io_battery");
        return;
    }

}

static void battery_step_frame(void)
{
    if (battery_paused) return; // don't perform updates while paused

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
    // 1. SEND RGB TIER MESSAGE (pointer pool)
    // -----------------------------
    uint8_t rgb_payload[5] = {0};
    const battery_rgb_tier_t* tier = battery_get_rgb_tier(voltage, vbus);
    if (tier) {
        rgb_payload[0] = tier->plugin;
        rgb_payload[1] = tier->h;
        rgb_payload[2] = tier->s;
        rgb_payload[3] = tier->v;
        rgb_payload[4] = tier->brightness;
    } else {
        rgb_payload[0] = RGB_PLUGIN_OFF;
    }

    dispatch_target_t rgb_targets[TARGET_MAX];
    dispatcher_fill_targets(rgb_targets);
    rgb_targets[0] = TARGET_RGB;
    dispatcher_pool_send_params_t params = {
        .type = DISPATCHER_POOL_STREAMING,
        .source = SOURCE_BATTERY,
        .targets = rgb_targets,
        .data = rgb_payload,
        .data_len = sizeof(rgb_payload),
        .context = NULL
    };
    dispatcher_pool_send_ptr_params(&params);

    // // -----------------------------
    // // 2. SEND LOG TEXT MESSAGE (pointer pool)
    // // -----------------------------
    // char log_buf[128];
    // size_t log_len = snprintf(
    //     log_buf,
    //     sizeof(log_buf),
    //     "VBUS=%d %s Voltage=%.2fV Percent=%d%%\r\n",
    //     vbus ? 1 : 0,
    //     state,
    //     voltage,
    //     percent
    // );

    // dispatch_target_t log_targets[TARGET_MAX];
    // dispatcher_fill_targets(log_targets);
    // log_targets[0] = TARGET_LOG;
    // dispatcher_pool_send_params_t log_params = {
    //     .type = DISPATCHER_POOL_STREAMING,
    //     .source = SOURCE_BATTERY,
    //     .targets = log_targets,
    //     .data = (const uint8_t *)log_buf,
    //     .data_len = log_len,
    //     .context = NULL
    // };
    // dispatcher_pool_send_ptr_params(&log_params);
}