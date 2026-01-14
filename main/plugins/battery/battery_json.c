#include "battery_json.h"
#include "rgb_anim.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include <stdlib.h>
#include "io_fatfs.h"

#define BATTERY_JSON_PATH "/data/Battery_Levels.json"
#define TAG "battery_json"

static battery_rgb_tier_t *tiers = NULL;
static size_t num_tiers = 0;

// Helper: map plugin string to rgb_plugin_id_t
static int plugin_name_to_id(const char *name) {
    for (int i = 0; i < RGB_PLUGIN_MAX; ++i) {
        if (strcmp(name, rgb_plugin_names[i] + 11) == 0) // skip "RGB_PLUGIN" prefix
            return i;
    }
    return -1;
}

// Helper: parse one array (charging/discharging)
static int parse_tier_array(cJSON *arr, bool vbus_required, battery_rgb_tier_t *out, int max) {
    int count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (count >= max) break;
        battery_rgb_tier_t t = {0};
        t.vbus_required = vbus_required;
        t.min_voltage = (float)cJSON_GetObjectItem(item, "min_voltage")->valuedouble;
        const char *plugin_str = cJSON_GetObjectItem(item, "plugin")->valuestring;
        int plugin_id = plugin_name_to_id(plugin_str);
        if (plugin_id < 0) continue;
        t.plugin = (uint8_t)plugin_id;
        t.h = (uint8_t)cJSON_GetObjectItem(item, "h")->valueint;
        t.s = (uint8_t)cJSON_GetObjectItem(item, "s")->valueint;
        t.v = (uint8_t)cJSON_GetObjectItem(item, "v")->valueint;
        t.brightness = (uint8_t)cJSON_GetObjectItem(item, "brightness")->valueint;
        out[count++] = t;
    }
    return count;
}

const battery_rgb_tier_t *battery_json_get_tiers(size_t *out_num) {
    if (out_num) *out_num = num_tiers;
    return tiers;
}


int battery_json_reload(void) {
    if (!io_fatfs_file_exists(BATTERY_JSON_PATH)) {
        ESP_LOGE(TAG, "File not found: %s", BATTERY_JSON_PATH);
        return -1;
    }
    // Estimate max file size (e.g. 2KB)
    size_t buf_size = 2048;
    char *buf = malloc(buf_size);
    if (!buf) {
        ESP_LOGE(TAG, "Out of memory");
        return -2;
    }
    int bytes_read = io_fatfs_read_file(BATTERY_JSON_PATH, (uint8_t *)buf, buf_size - 1);
    if (bytes_read < 0) {
        ESP_LOGE(TAG, "Failed to read file: %s", BATTERY_JSON_PATH);
        free(buf);
        return -3;
    }
    buf[bytes_read] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse error");
        return -4;
    }
    cJSON *charging = cJSON_GetObjectItem(root, "charging");
    cJSON *discharging = cJSON_GetObjectItem(root, "discharging");
    int max = 16; // arbitrary max
    battery_rgb_tier_t *new_tiers = malloc(sizeof(battery_rgb_tier_t) * max * 2);
    if (!new_tiers) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Out of memory for tiers");
        return -5;
    }
    int n = 0;
    if (charging) n += parse_tier_array(charging, true, new_tiers + n, max);
    if (discharging) n += parse_tier_array(discharging, false, new_tiers + n, max);
    cJSON_Delete(root);

    // Swap arrays
    battery_rgb_tier_t *old = tiers;
    tiers = new_tiers;
    num_tiers = n;
    if (old) free(old);
    ESP_LOGI(TAG, "Loaded %d battery tiers", n);
    return 0;
}
