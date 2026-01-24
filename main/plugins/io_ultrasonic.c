#include "io_ultrasonic.h"
#include "dispatcher.h"
#include "dispatcher_module.h"
#include "io_fatfs.h"
#include "cJSON.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define ULTRASONIC_JSON_PATH "/data/ultrasonic.json"

typedef struct {
    int trig_gpio;
    int echo_gpio;
    uint32_t trigger_pulse_us;
    uint32_t interval_ms;
    uint32_t min_distance_cm;
    uint32_t max_distance_cm;
    uint32_t rmt_resolution_hz;
    size_t rmt_mem_block_symbols;
    // derived
    uint32_t trigger_ticks;
    uint32_t period_ticks;
} ultrasonic_cfg_t;

static ultrasonic_cfg_t g_cfg;

static void io_ultrasonic_process_msg(const dispatcher_msg_t *msg);
static void io_ultrasonic_step_frame(void);

static dispatcher_module_t io_ultrasonic_mod = {
    .name = "io_ultrasonic",
    .target = TARGET_ULTRASONIC,
    .queue_len = 8,
    .stack_size = 4096,
    .task_prio = 5,
    .process_msg = io_ultrasonic_process_msg,
    .step_frame = io_ultrasonic_step_frame,
    .step_ms = CONFIG_ULTRASONIC_TRIGGER_INTERVAL_MS,
    .queue = NULL,
    .next_step = 0
};

static uint32_t cm_to_roundtrip_ns(uint32_t cm)
{
    const float v = 343.0f; // m/s at 20°C
    float meters = cm / 100.0f;
    return (uint32_t)((2.0f * meters / v) * 1e9f + 0.5f);
}

static uint32_t ns_to_ticks(uint32_t ns, uint32_t resolution_hz)
{
    return (uint32_t)(((uint64_t)ns * resolution_hz + 500000000ULL) / 1000000000ULL);
}

static void load_defaults_from_kconfig(ultrasonic_cfg_t *cfg)
{
    cfg->trig_gpio = CONFIG_ULTRASONIC_TRIG_GPIO;
    cfg->echo_gpio = CONFIG_ULTRASONIC_ECHO_GPIO;
    cfg->trigger_pulse_us = CONFIG_ULTRASONIC_TRIGGER_PULSE_US;
    cfg->interval_ms = CONFIG_ULTRASONIC_TRIGGER_INTERVAL_MS;
    cfg->min_distance_cm = CONFIG_ULTRASONIC_SIGNAL_RANGE_MIN_CM;
    cfg->max_distance_cm = CONFIG_ULTRASONIC_SIGNAL_RANGE_MAX_CM;
    cfg->rmt_resolution_hz = CONFIG_ULTRASONIC_RMT_RESOLUTION_HZ;
    cfg->rmt_mem_block_symbols = CONFIG_ULTRASONIC_RMT_MEM_BLOCK_SYMBOLS;
}

static bool load_config_from_json(ultrasonic_cfg_t *cfg)
{
    const size_t buf_size = 2048;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    if (!buf) {
        ESP_LOGW("io_ultrasonic", "malloc failed for json buffer");
        return false;
    }
    const char *p = ULTRASONIC_JSON_PATH;
    if (!io_fatfs_file_exists(p)) {
        free(buf);
        return false;
    }
    int read = io_fatfs_read_file(p, buf, buf_size - 1);
    if (read > 0) {
        buf[read] = '\0';
        ESP_LOGI("io_ultrasonic", "Loaded JSON config from %s (%d bytes)", p, read);
        cJSON *root = cJSON_Parse((const char *)buf);
        if (!root) {
            ESP_LOGE("io_ultrasonic", "Failed to parse JSON: %s", cJSON_GetErrorPtr());
            free(buf);
            return false;
        }
        cJSON *node = cJSON_GetObjectItem(root, "ultrasonic");
        if (node) {
            cJSON *item = NULL;
            item = cJSON_GetObjectItem(node, "trig_pin"); if (cJSON_IsNumber(item)) cfg->trig_gpio = item->valueint;
            item = cJSON_GetObjectItem(node, "echo_pin"); if (cJSON_IsNumber(item)) cfg->echo_gpio = item->valueint;
            item = cJSON_GetObjectItem(node, "trigger_pulse_us"); if (cJSON_IsNumber(item)) cfg->trigger_pulse_us = (uint32_t)item->valueint;
            item = cJSON_GetObjectItem(node, "min_distance_cm"); if (cJSON_IsNumber(item)) cfg->min_distance_cm = (uint32_t)item->valueint;
            item = cJSON_GetObjectItem(node, "max_distance_cm"); if (cJSON_IsNumber(item)) cfg->max_distance_cm = (uint32_t)item->valueint;
            item = cJSON_GetObjectItem(node, "rmt_resolution_hz"); if (cJSON_IsNumber(item)) cfg->rmt_resolution_hz = (uint32_t)item->valueint;
            item = cJSON_GetObjectItem(node, "rmt_mem_block_symbols"); if (cJSON_IsNumber(item)) cfg->rmt_mem_block_symbols = (size_t)item->valueint;
            cJSON_Delete(root);
            free(buf);
            return true;
        }
        cJSON_Delete(root);
    }
    free(buf);
    return false;
}

static void compute_derived(ultrasonic_cfg_t *cfg)
{
    // We use 1 RMT tick for trigger (user decision); ensure at least 1
    cfg->trigger_ticks = 1;
    cfg->period_ticks = (uint32_t)(((uint64_t)cfg->interval_ms * cfg->rmt_resolution_hz) / 1000);
    ESP_LOGI("io_ultrasonic", "CFG: trig=%d echo=%d interval=%ums res=%u Hz min=%dcm max=%dcm mem=%zu symbols",
             cfg->trig_gpio, cfg->echo_gpio, cfg->interval_ms, cfg->rmt_resolution_hz,
             cfg->min_distance_cm, cfg->max_distance_cm, cfg->rmt_mem_block_symbols);
    uint32_t max_rtt_ns = cm_to_roundtrip_ns(cfg->max_distance_cm);
    uint32_t max_ticks = ns_to_ticks(max_rtt_ns, cfg->rmt_resolution_hz);
    ESP_LOGI("io_ultrasonic", "Derived: max RTT %u ns -> ~%u ticks; period ticks=%u; trigger_ticks=%u",
             max_rtt_ns, max_ticks, cfg->period_ticks, cfg->trigger_ticks);
}

static void apply_config(void)
{
    compute_derived(&g_cfg);
    // Placeholder: apply GPIO/RMT setup when we flesh out module
}

static void io_ultrasonic_process_msg(const dispatcher_msg_t *msg)
{
    // Placeholder: handle runtime config updates (via dispatcher or JSON reload)
    (void)msg;
}

static void io_ultrasonic_step_frame(void)
{
    // Placeholder: periodic work (e.g., publish distance samples)
}

void io_ultrasonic_init(void)
{
    load_defaults_from_kconfig(&g_cfg);
    bool json_ok = load_config_from_json(&g_cfg);
    if (json_ok) {
        ESP_LOGI("io_ultrasonic", "Using JSON config for ultrasonic");
    } else {
        ESP_LOGI("io_ultrasonic", "Using Kconfig defaults for ultrasonic (no JSON found)");
    }
    apply_config();

    if (dispatcher_module_start(&io_ultrasonic_mod) != pdTRUE) {
        ESP_LOGE("io_ultrasonic", "Failed to start dispatcher module for io_ultrasonic");
        return;
    }
    ESP_LOGI("io_ultrasonic", "io_ultrasonic module started");
}
