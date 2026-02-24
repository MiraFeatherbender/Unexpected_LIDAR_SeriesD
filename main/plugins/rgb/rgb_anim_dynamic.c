#include "rgb_anim_dynamic.h"
#include "rgb_anim.h"
#include "rgb_core.h"
#include "io_rgb.h"
#include "io_fatfs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "noise_data.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if CONFIG_FREERTOS_UNICORE
#define DYNAMIC_TASK_CORE_ID 0
#else
#define DYNAMIC_TASK_CORE_ID 1
#endif

#define RGB_ANIM_JSON_PATH "/data/rgb_animations.json"
#define IMAGE_DIR "/data/images/"
#define STBI_ONLY_PNG

static uint8_t *buffer = NULL; // 8KB buffer for JSON file
static size_t buffer_len = 8192;

static uint8_t *dynamic_realloc_spiram8(uint8_t *ptr, size_t size)
{
    uint8_t *new_ptr = (uint8_t *)heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!new_ptr) {
        new_ptr = (uint8_t *)realloc(ptr, size);
    }
    return new_ptr;
}


// --- Animation Buffers ---
typedef struct {
    uint8_t  *palette_raw_rgb;             // 3x256 RGB palette
    size_t palette_raw_size;               // Size of palette_raw_rgb buffer
} rgb_anim_buffers_t;

static rgb_anim_buffers_t anim_buffers = {0};

typedef struct {
    noise_walk_spec_t spec;
    uint8_t x;
    uint8_t y;
} noise_walk_state_t;

// Opaque config struct definition
struct rgb_anim_dynamic_config {
    int id; // Plugin enum value
    char color_palette_png_path[256];
    char contrast_noise_png_path[256];
    char brightness_noise_png_path[256];
    noise_walk_spec_t contrast_walk_spec;
    noise_walk_spec_t brightness_walk_spec;
};

static noise_walk_state_t s_contrast_walk = {0};
static noise_walk_state_t s_brightness_walk = {0};
static uint8_t s_user_brightness = 255;
static int s_selected_plugin_id = -1;


// Static array of loaded configs
#define MAX_DYNAMIC_ANIMS 12
static rgb_anim_dynamic_config_t s_configs[MAX_DYNAMIC_ANIMS];
static int s_config_count = 0;
static uint8_t *s_palette_cache[MAX_DYNAMIC_ANIMS] = {0};
static size_t s_palette_cache_size[MAX_DYNAMIC_ANIMS] = {0};

// --- FreeRTOS Task & Semaphore ---
static SemaphoreHandle_t s_load_png_sem = NULL;
static int s_load_png_idx = -1; // Index to be used by the task
static TaskHandle_t s_load_png_task_handle = NULL;

// --- Helper: Read image file with prepended IMAGE_DIR ---
static int read_image_file(const char *file_member, uint8_t **out_buf, size_t *buf_size, int req_channels) {
    char fullpath[300];
    snprintf(fullpath, sizeof(fullpath), IMAGE_DIR "%s", file_member);

    int x,y,n;
    unsigned char *data = stbi_load(fullpath, &x, &y, &n, req_channels); // force req_channels
    if (!data) {
        ESP_LOGE("rgb_anim_dynamic", "Failed to load image file: %s", fullpath);
        return -1;
    }
    int read_bytes = x * y * req_channels; // since we forced req_channels
    if ((size_t)read_bytes > *buf_size) {
        uint8_t *new_buf = dynamic_realloc_spiram8(*out_buf, (size_t)read_bytes);
        if (!new_buf) {
            ESP_LOGE("rgb_anim_dynamic", "Failed to grow buffer to %d bytes for %s", read_bytes, fullpath);
            stbi_image_free(data);
            return -1;
        }
        *out_buf = new_buf;
        *buf_size = read_bytes;
    }

    memcpy(*out_buf, data, read_bytes);
    stbi_image_free(data);

    return read_bytes;
}

static int dynamic_find_config_index_by_plugin_id(uint8_t plugin_id) {
    for (int i = 0; i < s_config_count; ++i) {
        if (s_configs[i].id == (int)plugin_id) {
            return i;
        }
    }
    return -1;
}

static void dynamic_palette_cache_clear(void) {
    for (int i = 0; i < MAX_DYNAMIC_ANIMS; ++i) {
        if (s_palette_cache[i]) {
            free(s_palette_cache[i]);
            s_palette_cache[i] = NULL;
        }
        s_palette_cache_size[i] = 0;
    }
    anim_buffers.palette_raw_rgb = NULL;
    anim_buffers.palette_raw_size = 0;
}

static void Load_PNG_Task(void *pvParameters) {
    while (1) {
        // Wait for semaphore to be given
        if (xSemaphoreTake(s_load_png_sem, portMAX_DELAY) == pdTRUE) {
            int idx = s_load_png_idx;
            if (idx >= 0 && idx < s_config_count) {
                uint8_t *palette = NULL;
                size_t palette_size = 0;
                int bytes = read_image_file(s_configs[idx].color_palette_png_path, &palette, &palette_size, 3);
                if (bytes <= 0 || !palette) {
                    if (palette) {
                        free(palette);
                    }
                    continue;
                }

                if (s_palette_cache[idx]) {
                    free(s_palette_cache[idx]);
                }
                s_palette_cache[idx] = palette;
                s_palette_cache_size[idx] = (size_t)bytes;

                anim_buffers.palette_raw_rgb = s_palette_cache[idx];
                anim_buffers.palette_raw_size = s_palette_cache_size[idx];

                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}

// Forward declarations for plugin interface
static void dynamic_begin(uint8_t *phase_u8);
static void dynamic_step(rgb_color_t *out_rgb);
static void dynamic_set_color(rgb_color_t rgb);
static void dynamic_set_brightness(uint8_t b);
static bool dynamic_sample_rgb(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);

static rgb_anim_t s_dynamic_anim = {
    .begin = dynamic_begin,
    .step = dynamic_step,
    .set_color = dynamic_set_color,
    .set_brightness = dynamic_set_brightness,
};

static rgb_anim_ex_t s_dynamic_anim_ex = {
    .begin_phase = NULL,
    .set_brightness = NULL,
    .sample_rgb = dynamic_sample_rgb,
};

// Active config index
static int s_active_idx = 0;

// Helper macros
#define GET_INT(obj, key, def) ({ cJSON *it = cJSON_GetObjectItem(obj, key); (it && cJSON_IsNumber(it)) ? it->valueint : (def); })
#define GET_STR(obj, key, dest) do { cJSON *it = cJSON_GetObjectItem(obj, key); if (it && cJSON_IsString(it)) strncpy(dest, it->valuestring, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
static noise_walk_spec_t parse_walk_spec(cJSON *obj) {
    noise_walk_spec_t spec = {0};
    if (obj) {
        spec.min_dx = GET_INT(obj, "min_dx", 0);
        spec.max_dx = GET_INT(obj, "max_dx", 0);
        spec.min_dy = GET_INT(obj, "min_dy", 0);
        spec.max_dy = GET_INT(obj, "max_dy", 0);
    }
    return spec;
}

static bool dynamic_map_noise_u8_to_rgb(uint8_t plugin_id, uint8_t noise_u8, rgb_color_t *out_rgb) {
    if (!out_rgb) {
        return false;
    }

    const uint8_t *palette_raw = NULL;
    size_t palette_raw_size = 0;

    int cfg_idx = dynamic_find_config_index_by_plugin_id(plugin_id);
    if (cfg_idx >= 0 && s_palette_cache[cfg_idx] && s_palette_cache_size[cfg_idx] >= 3) {
        palette_raw = s_palette_cache[cfg_idx];
        palette_raw_size = s_palette_cache_size[cfg_idx];
    }

    // Compatibility fallback: use currently active/loaded palette if per-plugin cache unavailable.
    if ((!palette_raw || palette_raw_size < 3) && anim_buffers.palette_raw_rgb && anim_buffers.palette_raw_size >= 3) {
        palette_raw = anim_buffers.palette_raw_rgb;
        palette_raw_size = anim_buffers.palette_raw_size;
    }

    if (!palette_raw || palette_raw_size < 3) {
        return false;
    }

    size_t palette_count = palette_raw_size / 3;
    if (palette_count == 0) {
        return false;
    }

    size_t palette_idx = ((size_t)noise_u8 * (palette_count - 1)) / 255;
    const uint8_t *palette = palette_raw + (palette_idx * 3);
    out_rgb->r = palette[0];
    out_rgb->g = palette[1];
    out_rgb->b = palette[2];
    return true;
}

void rgb_anim_dynamic_init(void) {
    if (!buffer) {
        buffer = (uint8_t *)heap_caps_malloc(buffer_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buffer) {
            buffer = (uint8_t *)malloc(buffer_len);
        }
        if (!buffer) {
            ESP_LOGE("rgb_anim_dynamic", "Failed to allocate JSON buffer");
        }
    }
    // Load and parse JSON config file
    rgb_anim_dynamic_reload();

    // For each loaded animation, register with io_rgb
    for (int i = 0; i < s_config_count; ++i) {
        io_rgb_register_rgb_plugin_ex(s_configs[i].id, &s_dynamic_anim_ex);
        io_rgb_register_rgb_plugin(s_configs[i].id, &s_dynamic_anim);
    }

    // Create semaphore and task for PNG loading
    if (!s_load_png_sem) {
        s_load_png_sem = xSemaphoreCreateBinary();
    }
    if (s_load_png_task_handle == NULL) {
        xTaskCreatePinnedToCore(Load_PNG_Task, "Load_PNG_Task", 16384, NULL, 5, &s_load_png_task_handle, DYNAMIC_TASK_CORE_ID);
    }
}

bool rgb_anim_dynamic_reload(void) {
    if (!buffer) {
        ESP_LOGE("rgb_anim_dynamic", "JSON buffer not allocated");
        return false;
    }
    int read_bytes = io_fatfs_read_file(RGB_ANIM_JSON_PATH, buffer, buffer_len - 1);
    if (read_bytes <= 0) {
        // Failed to read JSON file
        return false;
    }
    buffer[read_bytes] = '\0'; // Null-terminate for cJSON

    dynamic_palette_cache_clear();

    cJSON *root = cJSON_Parse((char *)buffer);
    if (!root) {
        // JSON parsing error
        return false;
    }

    cJSON *animations = cJSON_GetObjectItem(root, "animations");
    int count = cJSON_GetArraySize(animations);
    s_config_count = count > MAX_DYNAMIC_ANIMS ? MAX_DYNAMIC_ANIMS : count;
    for (int i = 0; i < s_config_count; i++) {
        cJSON *anim = cJSON_GetArrayItem(animations, i);
        s_configs[i].id = GET_INT(anim, "id", 0);
        GET_STR(anim, "palette", s_configs[i].color_palette_png_path);
        GET_STR(anim, "contrast_noise_field", s_configs[i].contrast_noise_png_path);
        GET_STR(anim, "brightness_noise_field", s_configs[i].brightness_noise_png_path);
        s_configs[i].contrast_walk_spec = parse_walk_spec(cJSON_GetObjectItem(anim, "contrast_walk_spec"));
        s_configs[i].brightness_walk_spec = parse_walk_spec(cJSON_GetObjectItem(anim, "brightness_walk_spec"));
    }

    cJSON_Delete(root);
    return true;
}

int rgb_anim_dynamic_count(void) {
    return s_config_count;
}

void rgb_anim_dynamic_select_plugin(uint8_t plugin_id) {
    s_selected_plugin_id = (int)plugin_id;
}

void rgb_anim_dynamic_request_preload(uint8_t plugin_id) {
    int cfg_idx = dynamic_find_config_index_by_plugin_id(plugin_id);
    if (cfg_idx < 0) {
        return;
    }

    s_load_png_idx = cfg_idx;
    if (s_load_png_sem) {
        xSemaphoreGive(s_load_png_sem);
    }
}

// --- Plugin interface implementations ---
static void dynamic_begin(uint8_t *phase_u8) {
    if (phase_u8) {
        *phase_u8 = 0;
    }

    int idx = s_selected_plugin_id;
    if (idx < 0) {
        idx = 0;
    }

    // Set the active config index for this plugin instance
    s_active_idx = idx;
    for(int i = 0; i < s_config_count; i++) {
        if (s_configs[i].id == idx) {
            s_active_idx = i;
            break;
        }
    }

    // Load walk specs from config and reset walk positions
    s_contrast_walk.spec = s_configs[s_active_idx].contrast_walk_spec;
    s_brightness_walk.spec = s_configs[s_active_idx].brightness_walk_spec;
    s_contrast_walk.x = 128;
    s_contrast_walk.y = 128;
    s_brightness_walk.x = 128;
    s_brightness_walk.y = 128;

    // Trigger the PNG load task with the selected idx
    s_load_png_idx = s_active_idx;
    if (s_load_png_sem) {
        xSemaphoreGive(s_load_png_sem);
    }

    // TODO: State machine logic will be handled in Load_PNG_Task
}

static void dynamic_step(rgb_color_t *out_rgb) {
    if (!out_rgb) {
        return;
    }

    if (s_config_count <= 0 || s_active_idx < 0 || s_active_idx >= s_config_count) {
        out_rgb->r = 0;
        out_rgb->g = 0;
        out_rgb->b = 0;
        return;
    }

    uint8_t plugin_id = (uint8_t)s_configs[s_active_idx].id;
    uint8_t noise_u8 = s_contrast_walk.x;

    if (!dynamic_map_noise_u8_to_rgb(plugin_id, noise_u8, out_rgb)) {
        out_rgb->r = 0;
        out_rgb->g = 0;
        out_rgb->b = 0;
    }

    if (s_user_brightness < 255) {
        out_rgb->r = (uint8_t)(((uint16_t)out_rgb->r * s_user_brightness) >> 8);
        out_rgb->g = (uint8_t)(((uint16_t)out_rgb->g * s_user_brightness) >> 8);
        out_rgb->b = (uint8_t)(((uint16_t)out_rgb->b * s_user_brightness) >> 8);
    }

    io_rgb_set_anim_brightness(s_user_brightness);

    noise_walk_step(&s_contrast_walk.x, &s_contrast_walk.y, &s_contrast_walk.spec);
    noise_walk_step(&s_brightness_walk.x, &s_brightness_walk.y, &s_brightness_walk.spec);
}

static void dynamic_set_color(rgb_color_t rgb) {
    (void)rgb;
    // No-op: dynamic plugins are fully defined by JSON/PNG inputs.
}

static void dynamic_set_brightness(uint8_t b) {
    s_user_brightness = b;
}

static bool dynamic_sample_rgb(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb) {
    if (!in || !out_rgb) {
        return false;
    }

    if (!dynamic_map_noise_u8_to_rgb(in->plugin_id, in->noise_u8, out_rgb)) {
        return false;
    }

    if (in->brightness < 255) {
        out_rgb->r = (uint8_t)(((uint16_t)out_rgb->r * in->brightness) >> 8);
        out_rgb->g = (uint8_t)(((uint16_t)out_rgb->g * in->brightness) >> 8);
        out_rgb->b = (uint8_t)(((uint16_t)out_rgb->b * in->brightness) >> 8);
    }

    return true;
}
