#include "dispatcher_allocator.h"
#include "io_fatfs.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "dispatcher_allocator";
static const char *CONFIG_PATH = "/data/dispatcher_pool_config.json";

// Default values
#define DEFAULT_F 0.25
#define DEFAULT_C 2
#define DEFAULT_PAYLOAD 128
#define DEFAULT_MIN_ENTRIES 8
#define DEFAULT_MAX_ENTRIES 512

static pool_config_t streaming_cfg = { DEFAULT_F, DEFAULT_C, DEFAULT_PAYLOAD, DEFAULT_MIN_ENTRIES, DEFAULT_MAX_ENTRIES };
static pool_config_t control_cfg = { DEFAULT_F, DEFAULT_C, DEFAULT_PAYLOAD, DEFAULT_MIN_ENTRIES, DEFAULT_MAX_ENTRIES };

static double get_double_field(cJSON *obj, const char *name, double def) {
    if (!obj) return def;
    cJSON *it = cJSON_GetObjectItem(obj, name);
    if (!it || !cJSON_IsNumber(it)) return def;
    return it->valuedouble;
}
static int get_int_field(cJSON *obj, const char *name, int def) {
    if (!obj) return def;
    cJSON *it = cJSON_GetObjectItem(obj, name);
    if (!it || !cJSON_IsNumber(it)) return def;
    return it->valueint;
}

static void parse_pool_object(cJSON *root, const char *key, pool_config_t *out) {
    if (!root || !out) return;
    cJSON *pools = cJSON_GetObjectItem(root, "pools");
    if (!pools) return;
    cJSON *obj = cJSON_GetObjectItem(pools, key);
    if (!obj) return;

    double f = get_double_field(obj, "F", out->F);
    int c = get_int_field(obj, "C", out->C);
    int payload = get_int_field(obj, "payload_size", out->payload_size);
    int min_e = get_int_field(obj, "min_entries", out->min_entries);
    int max_e = get_int_field(obj, "max_entries", out->max_entries);

    // Validate
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    if (c < 1) c = 1;
    if (payload < 16) payload = out->payload_size;
    if (min_e < 1) min_e = out->min_entries;
    if (max_e < min_e) max_e = out->max_entries;

    out->F = f;
    out->C = c;
    out->payload_size = payload;
    out->min_entries = min_e;
    out->max_entries = max_e;
}

int dispatcher_allocator_load_config(void) {
    if (!io_fatfs_file_exists(CONFIG_PATH)) {
        ESP_LOGW(TAG, "Config file not found: %s (using defaults)", CONFIG_PATH);
        return -1;
    }

    size_t buf_size = 4096;
    char *buf = (char *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = (char *)malloc(buf_size);
    }
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for config parse");
        return -2;
    }

    int r = io_fatfs_read_file(CONFIG_PATH, (uint8_t *)buf, buf_size - 1);
    if (r < 0) {
        ESP_LOGE(TAG, "Failed to read config file: %s", CONFIG_PATH);
        free(buf);
        return -3;
    }
    buf[r] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON config");
        return -4;
    }

    // Copy defaults then parse
    pool_config_t s = streaming_cfg;
    pool_config_t c = control_cfg;
    parse_pool_object(root, "streaming", &s);
    parse_pool_object(root, "control", &c);

    cJSON_Delete(root);

    // Swap in
    streaming_cfg = s;
    control_cfg = c;

    ESP_LOGI(TAG, "Loaded dispatcher pool config:");
    ESP_LOGI(TAG, " streaming: F=%.3f C=%d payload=%d min=%d max=%d",
             streaming_cfg.F, streaming_cfg.C, streaming_cfg.payload_size, streaming_cfg.min_entries, streaming_cfg.max_entries);
    ESP_LOGI(TAG, " control:   F=%.3f C=%d payload=%d min=%d max=%d",
             control_cfg.F, control_cfg.C, control_cfg.payload_size, control_cfg.min_entries, control_cfg.max_entries);



    return 0;
}

void dispatcher_allocator_init(void) {
    // Initialize config (load file if exists)
    if (dispatcher_allocator_load_config() == 0) {
        ESP_LOGI(TAG, "dispatcher_allocator configuration loaded");
    } else {
        ESP_LOGI(TAG, "dispatcher_allocator using defaults");
    }
}

const pool_config_t *dispatcher_allocator_get_streaming_config(void) {
    return &streaming_cfg;
}
const pool_config_t *dispatcher_allocator_get_control_config(void) {
    return &control_cfg;
}