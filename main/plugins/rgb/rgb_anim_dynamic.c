#include "rgb_anim_dynamic.h"
#include "rgb_anim.h"
#include "io_rgb.h"
#include "io_fatfs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_dsp.h" // for convolution operations
#include "esp_heap_caps.h"
#include "noise_data.h"
#include "rgb_dynamic_fnl_config.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define RGB_ANIM_JSON_ONBOARD_PATH "/data/rgb_animations_onboard.json"
#define RGB_ANIM_JSON_STRIP_PATH   "/data/rgb_animations_strip.json"
#define RGB_ANIM_JSON_LEGACY_PATH  "/data/rgb_animations.json"
#define IMAGE_DIR "/data/images/"
#define STBI_ONLY_PNG

// --- Configurable blur macros (change here, no rebuild required for small edits) ---
#define DYN_BLUR_SEPARABLE 1
// must be odd
#define DYN_BLUR_KERNEL_SIZE 5
#define DYN_BLUR_SIGMA 1.0f

static uint8_t *buffer = NULL; // 8KB buffer for JSON file
static size_t buffer_len = 8192;


// --- Animation Buffers ---
typedef struct {
    rgb_color_t *contrast_rgb_active;      // Final, palette-mapped, blurred contrast noise (RGB, struct)
    rgb_color_t *contrast_rgb_staging;     // Staging for contrast RGB (for blur, swap, etc., struct)
    rgb_color_t *contrast_rgb_colored;     // Palette-mapped, unblurred contrast noise (RGB, struct)
    uint8_t  *contrast_gray_init;          // Initial grayscale load for contrast
    uint8_t  *brightness_gray_active;      // Final, blurred brightness noise (grayscale)
    uint8_t  *brightness_gray_staging;     // Staging for brightness grayscale (for blur, swap, etc.)
    uint8_t  *brightness_gray_init;        // Initial grayscale load for brightness
    uint8_t  *palette_raw_rgb;             // 3x256 RGB palette
    float *padded_blur_buf;                // Padding buffer for blur operations (float for ESP-DSP)
    float *unpadded_blur_buf;              // Unpadded buffer for blur operations (float for ESP-DSP)
    float *channel_float_src;              // temporary float buffer for per-channel linear values (W*H)
    float *palette_linear;                 // palette stored as linear floats (Nx3)
    size_t contrast_gray_size;             // Size of contrast_gray_init buffer
    size_t brightness_gray_size;           // Size of brightness_gray_init buffer
    size_t palette_raw_size;               // Size of palette_raw_rgb buffer
} rgb_anim_buffers_t;

static rgb_anim_buffers_t anim_buffers = {0};

// Helper function: allocates memory and logs error if allocation fails
static bool alloc_buffer(void **ptr, size_t size, const char *name) {
    *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!*ptr) {
        *ptr = malloc(size);
    }
    if (!*ptr) {
        ESP_LOGE("rgb_anim_dynamic", "Failed to allocate %s (%u bytes)", name, (unsigned)size);
        return false;
    }
    return true;
}

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
    rgb_dynamic_anim_mode_t noise_mode;
    fnl_state fnl;
};

static noise_walk_state_t s_contrast_walk = {0};
static noise_walk_state_t s_brightness_walk = {0};
static uint8_t s_user_brightness = 255;


// Static array of loaded configs
static rgb_anim_dynamic_config_t s_configs[RGB_DYNAMIC_MAX_ANIMS];
static rgb_anim_dynamic_config_t s_reload_scratch[RGB_DYNAMIC_MAX_ANIMS];
static int s_config_count = 0;
static rgb_anim_dynamic_config_source_t s_config_source = RGB_DYNAMIC_CONFIG_ONBOARD;

// --- FreeRTOS Task & Semaphore ---
static SemaphoreHandle_t s_load_png_sem = NULL;
static int s_load_png_idx = -1; // Index to be used by the task
static TaskHandle_t s_load_png_task_handle = NULL;
static const uint32_t s_load_png_task_stack = 16384;

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
    size_t capacity = *buf_size;
    if ((size_t)read_bytes > capacity) {
        uint8_t *new_buf = (uint8_t *)heap_caps_realloc(*out_buf, read_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!new_buf) {
            new_buf = (uint8_t *)realloc(*out_buf, read_bytes);
        }
        if (!new_buf) {
            ESP_LOGE("rgb_anim_dynamic", "Failed to grow buffer to %d bytes for %s", read_bytes, fullpath);
            stbi_image_free(data);
            return -1;
        }
        *out_buf = new_buf;
    }

    memcpy(*out_buf, data, read_bytes);
    *buf_size = (size_t)read_bytes;
    stbi_image_free(data);

    return read_bytes;
}

// Helper: swap two pointers
static void swap_ptrs(void **a, void **b) {
    void *temp = *a;
    *a = *b;
    *b = temp;
}

// src: pointer to input 8-bit buffer (size: width*height)
// dst: pointer to output 8-bit buffer (size: width*height)
// kernel: 3x3 array of uint8_t weights (row-major order) 
// kernel_div: sum of all kernel weights (for normalization) 
// width, height: image dimensions

#define BLUR_KERNEL_WEIGHTS (float[]){1/16.0f,2/16.0f,1/16.0f,2/16.0f,4/16.0f,2/16.0f,1/16.0f,2/16.0f,1/16.0f} // Gaussian blur kernel (sum to 1)

// Pad the source image into the padded buffer (with wraparound), with stride for struct channel access
void pad_image_u8_strided(const uint8_t *input, float *padded, int image_width, int image_height, int stride) {
    int padded_width = image_width + 2, padded_height = image_height + 2;
    // Center
    for (int row = 0; row < image_height; ++row)
        for (int col = 0; col < image_width; ++col)
            padded[(row + 1) * padded_width + (col + 1)] = (float)input[(row * image_width + col) * stride];
    // Top row (wrap)
    for (int col = 0; col < image_width; ++col)
        padded[0 * padded_width + 1 + col] = (float)input[((image_height - 1) * image_width + col) * stride];
    // Bottom row (wrap)
    for (int col = 0; col < image_width; ++col)
        padded[(padded_height - 1) * padded_width + 1 + col] = (float)input[col * stride];
    // Left/right columns (wrap, including ghost rows)
    for (int row = 0; row < padded_height; ++row) {
        // Copy last real column to ghost left column
        padded[row * padded_width + 0] = padded[row * padded_width + image_width];
        // Copy first real column to ghost right column
        padded[row * padded_width + padded_width - 1] = padded[row * padded_width + 1];
    }
}

// Pad float single-channel image into padded buffer with wraparound
void pad_image_f(const float *input, float *padded, int image_width, int image_height) {
    int padded_width = image_width + 2, padded_height = image_height + 2;
    // Center
    for (int row = 0; row < image_height; ++row)
        for (int col = 0; col < image_width; ++col)
            padded[(row + 1) * padded_width + (col + 1)] = input[row * image_width + col];
    // Top row (wrap)
    for (int col = 0; col < image_width; ++col)
        padded[0 * padded_width + 1 + col] = input[((image_height - 1) * image_width + col)];
    // Bottom row (wrap)
    for (int col = 0; col < image_width; ++col)
        padded[(padded_height - 1) * padded_width + 1 + col] = input[col];
    // Left/right columns (wrap, including ghost rows)
    for (int row = 0; row < padded_height; ++row) {
        // Copy last real column to ghost left column
        padded[row * padded_width + 0] = padded[row * padded_width + image_width];
        // Copy first real column to ghost right column
        padded[row * padded_width + padded_width - 1] = padded[row * padded_width + 1];
    }
}

void blur_image_simd(const float *padded_input, const float *blur_kernel, int image_width, int image_height) {
    int padded_width = image_width + 2;
    int padded_height = image_height + 2;

    image2d_t input_img = {
        .data = (void *)padded_input,
        .step_x = 1,
        .step_y = 1,
        .stride_x = padded_width,
        .stride_y = padded_height,
        .size_x = padded_width,
        .size_y = padded_height
    };

    image2d_t output_img = {
        .data = (void *)anim_buffers.unpadded_blur_buf,
        .step_x = 1,
        .step_y = 1,
        .stride_x = padded_width,
        .stride_y = padded_height,
        .size_x = padded_width,
        .size_y = padded_height
    };

    image2d_t blur_kernel_img = {
        .data = (void *)blur_kernel,
        .step_x = 1,
        .step_y = 1,
        .stride_x = 3,
        .stride_y = 3,
        .size_x = 3,
        .size_y = 3
    };

    dspi_conv_f32(&input_img, &blur_kernel_img, &output_img);

}

// Build a 1-D Gaussian kernel (size must be odd)
static void build_gaussian_1d(float *k, int ksize, float sigma) {
    int r = ksize / 2;
    float sum = 0.0f;
    for (int i = 0; i < ksize; ++i) {
        float x = (float)(i - r);
        float v = expf(-0.5f * (x * x) / (sigma * sigma));
        k[i] = v;
        sum += v;
    }
    if (sum > 0.0f) {
        for (int i = 0; i < ksize; ++i) k[i] /= sum;
    }
}

// Separable blur (wraparound) on float src (W*H). tmp must be W*H, dst must be W*H.
static void separable_blur_float(const float *src, float *tmp, float *dst, int W, int H, const float *k, int ksize) {
    // Use DSP convolution in two 1-D passes to leverage SIMD.
    int padded_w = W + 2;
    int padded_h = H + 2;

    // pad src into padded buffer
    pad_image_f(src, anim_buffers.padded_blur_buf, W, H);

    // Prepare image descriptors
    image2d_t input_img = {
        .data = (void *)anim_buffers.padded_blur_buf,
        .step_x = 1,
        .step_y = 1,
        .stride_x = padded_w,
        .stride_y = padded_h,
        .size_x = padded_w,
        .size_y = padded_h
    };

    image2d_t output_img = {
        .data = (void *)anim_buffers.unpadded_blur_buf,
        .step_x = 1,
        .step_y = 1,
        .stride_x = padded_w,
        .stride_y = padded_h,
        .size_x = padded_w,
        .size_y = padded_h
    };

    // Horizontal kernel: ksize x 1
    image2d_t kernel_h = {
        .data = (void *)k,
        .step_x = 1,
        .step_y = 1,
        .stride_x = ksize,
        .stride_y = 1,
        .size_x = ksize,
        .size_y = 1
    };

    // First pass: horizontal
    dspi_conv_f32(&input_img, &kernel_h, &output_img);

    // Second pass: vertical; use unpadded_blur_buf as input, padded_blur_buf as output
    image2d_t input_img_v = {
        .data = (void *)anim_buffers.unpadded_blur_buf,
        .step_x = 1,
        .step_y = 1,
        .stride_x = padded_w,
        .stride_y = padded_h,
        .size_x = padded_w,
        .size_y = padded_h
    };
    image2d_t output_img_v = {
        .data = (void *)anim_buffers.padded_blur_buf,
        .step_x = 1,
        .step_y = 1,
        .stride_x = padded_w,
        .stride_y = padded_h,
        .size_x = padded_w,
        .size_y = padded_h
    };
    image2d_t kernel_v = {
        .data = (void *)k,
        .step_x = 1,
        .step_y = 1,
        .stride_x = 1,
        .stride_y = ksize,
        .size_x = 1,
        .size_y = ksize
    };

    dspi_conv_f32(&input_img_v, &kernel_v, &output_img_v);

    // Final result is in anim_buffers.padded_blur_buf (padded). Copy to dst as flattened HxW center region
    int pw = padded_w;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            dst[y * W + x] = anim_buffers.padded_blur_buf[(y + 1) * pw + (x + 1)];
        }
    }
}

// Blur a single-channel float source into anim_buffers.unpadded_blur_buf and then
// scatter center region into dst (float -> remains floats in unpadded_blur_buf center)
void blur_channel_float_and_scatter(const float *src, uint8_t *dst_uint8, int width, int height, int stride) {
    // Choose separable CPU path or existing SIMD 2D conv
#if DYN_BLUR_SEPARABLE
    // build 1D kernel
    const int ksize = DYN_BLUR_KERNEL_SIZE;
    float k[ksize];
    build_gaussian_1d(k, ksize, DYN_BLUR_SIGMA);
    // use anim_buffers.unpadded_blur_buf as destination (W*H fits)
    float *dstf = anim_buffers.unpadded_blur_buf; // large enough
    float *tmpf = anim_buffers.channel_float_src; // reuse as temporary
    separable_blur_float(src, tmpf, dstf, width, height, k, ksize);
    // scatter
        for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int src_idx = row * width + col;
            int dst_idx = (row * width + col) * stride;
            float lin = dstf[src_idx];
            // linear (0..1) -> sRGB (0..1)
            float s;
            if (lin <= 0.0f) s = 0.0f;
            else if (lin <= 0.0031308f) s = 12.92f * lin;
            else s = 1.055f * powf(lin, 1.0f/2.4f) - 0.055f;
            int v = (int)fminf(fmaxf(s * 255.0f, 0.0f), 255.0f);
            dst_uint8[dst_idx] = (uint8_t)v;
        	}
    }
#else
    // pad float input into padded buffer
    pad_image_f(src, anim_buffers.padded_blur_buf, width, height);
    // run SIMD convolution (reads padded, writes anim_buffers.unpadded_blur_buf)
    blur_image_simd(anim_buffers.padded_blur_buf, BLUR_KERNEL_WEIGHTS, width, height);
    // Scatter float output into destination (convert linear->sRGB8)
    int padded_width = width + 2;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int src_idx = (row + 1) * padded_width + (col + 1);
            int dst_idx = (row * width + col) * stride;
            float lin = anim_buffers.unpadded_blur_buf[src_idx];
            // linear (0..1) -> sRGB (0..1)
            float s;
            if (lin <= 0.0f) s = 0.0f;
            else if (lin <= 0.0031308f) s = 12.92f * lin;
            else s = 1.055f * powf(lin, 1.0f/2.4f) - 0.055f;
            int v = (int)fminf(fmaxf(s * 255.0f, 0.0f), 255.0f);
            dst_uint8[dst_idx] = (uint8_t)v;
        }
    }
#endif
}

// Apply 3x3 blur to single channel 8-bit image, with stride for struct channel access
void blur_channel_strided(const uint8_t *src, uint8_t *dst, float *padded, int width, int height, int stride) {
    pad_image_u8_strided(src, padded, width, height, stride);
    blur_image_simd(padded, BLUR_KERNEL_WEIGHTS, width, height);
    // Scatter float output into destination using stride (center region only)
    int padded_width = width + 2;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int src_idx = (row + 1) * padded_width + (col + 1);
            int dst_idx = (row * width + col) * stride;
            dst[dst_idx] = (uint8_t)fminf(fmaxf(anim_buffers.unpadded_blur_buf[src_idx], 0.0f), 255.0f);
        }
    }
}

static void Load_PNG_Task(void *pvParameters) {
    while (1) {
        // Wait for semaphore to be given
        if (xSemaphoreTake(s_load_png_sem, portMAX_DELAY) == pdTRUE) {
            int idx = s_load_png_idx;
            if (idx >= 0 && idx < s_config_count) {
                int bytes = read_image_file(s_configs[idx].color_palette_png_path,
                                            &anim_buffers.palette_raw_rgb,
                                            &anim_buffers.palette_raw_size,
                                            3);
                if (bytes <= 0) {
                    ESP_LOGE("rgb_anim_dynamic", "Failed to load palette PNG for config id=%d", s_configs[idx].id);
                }
            }
        }
    }
}

// Forward declarations for plugin interface
static void dynamic_begin(int idx);
static void dynamic_step(rgb_color_t *out_rgb);
static void dynamic_set_color(rgb_color_t rgb);
static void dynamic_set_brightness(uint8_t b);

static rgb_anim_t s_dynamic_anim = {
    .begin = dynamic_begin,
    .step = dynamic_step,
    .set_color = dynamic_set_color,
    .set_brightness = dynamic_set_brightness,
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

static rgb_dynamic_anim_mode_t parse_anim_mode(cJSON *anim)
{
    cJSON *mode = cJSON_GetObjectItem(anim, "anim_mode");
    if (!mode) {
        mode = cJSON_GetObjectItem(anim, "noise_mode");
    }
    if (!mode || !cJSON_IsString(mode) || !mode->valuestring) {
        return RGB_DYNAMIC_ANIM_MODE_DYNAMIC;
    }

    // Keep legacy JSON string mapping for compatibility.
    if (strcmp(mode->valuestring, "static") == 0) {
        return RGB_DYNAMIC_ANIM_MODE_DEDICATED;
    }

    return RGB_DYNAMIC_ANIM_MODE_DYNAMIC;
}

static void parse_fnl_state_with_defaults(cJSON *fnl_obj, fnl_state *out);

static const char *config_primary_path(rgb_anim_dynamic_config_source_t source)
{
    return (source == RGB_DYNAMIC_CONFIG_STRIP) ? RGB_ANIM_JSON_STRIP_PATH : RGB_ANIM_JSON_ONBOARD_PATH;
}

static const char *config_fallback1_path(rgb_anim_dynamic_config_source_t source)
{
    return (source == RGB_DYNAMIC_CONFIG_STRIP) ? RGB_ANIM_JSON_ONBOARD_PATH : RGB_ANIM_JSON_LEGACY_PATH;
}

static const char *config_fallback2_path(rgb_anim_dynamic_config_source_t source)
{
    return (source == RGB_DYNAMIC_CONFIG_STRIP) ? RGB_ANIM_JSON_LEGACY_PATH : NULL;
}

static int find_config_index_by_id(const rgb_anim_dynamic_config_t *cfgs, int count, int id)
{
    for (int i = 0; i < count; ++i) {
        if (cfgs[i].id == id) return i;
    }
    return -1;
}

static bool write_text_file(const char *path, const char *text)
{
    if (!path || !text) return false;

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGW("rgb_anim_dynamic", "Failed to open %s for write", path);
        return false;
    }

    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, f);
    fclose(f);
    if (written != len) {
        ESP_LOGW("rgb_anim_dynamic", "Short write for %s (%u/%u)", path, (unsigned)written, (unsigned)len);
        return false;
    }

    return true;
}

static bool is_mode_string_valid(const cJSON *mode)
{
    return mode && cJSON_IsString(mode) && mode->valuestring &&
           ((strcmp(mode->valuestring, "static") == 0) || (strcmp(mode->valuestring, "fnl") == 0));
}

static bool load_configs_from_file(const char *path, rgb_anim_dynamic_config_t *out_cfgs, int *out_count)
{
    if (!path || !out_cfgs || !out_count || !buffer) return false;
    bool rewrite_legacy_key = (strcmp(path, RGB_ANIM_JSON_LEGACY_PATH) != 0);
    bool rewrite_performed = false;

    int read_bytes = io_fatfs_read_file(path, buffer, buffer_len - 1);
    if (read_bytes <= 0) {
        return false;
    }
    buffer[read_bytes] = '\0';

    cJSON *root = cJSON_Parse((char *)buffer);
    if (!root) {
        return false;
    }

    cJSON *animations = cJSON_GetObjectItem(root, "animations");
    if (!animations || !cJSON_IsArray(animations)) {
        cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(animations);
    if (count > RGB_DYNAMIC_MAX_ANIMS) count = RGB_DYNAMIC_MAX_ANIMS;

    for (int i = 0; i < count; i++) {
        cJSON *anim = cJSON_GetArrayItem(animations, i);
        cJSON *anim_mode_obj = cJSON_GetObjectItem(anim, "anim_mode");
        cJSON *noise_mode_obj = cJSON_GetObjectItem(anim, "noise_mode");
        cJSON *fnl_state_obj = cJSON_GetObjectItem(anim, "fnl_state");

        if (rewrite_legacy_key && !anim_mode_obj && is_mode_string_valid(noise_mode_obj)) {
            cJSON_AddStringToObject(anim, "anim_mode", noise_mode_obj->valuestring);
            cJSON_DeleteItemFromObject(anim, "noise_mode");
            anim_mode_obj = cJSON_GetObjectItem(anim, "anim_mode");
            noise_mode_obj = NULL;
            rewrite_performed = true;
        }

        out_cfgs[i].id = GET_INT(anim, "id", 0);
        GET_STR(anim, "palette", out_cfgs[i].color_palette_png_path);
        GET_STR(anim, "contrast_noise_field", out_cfgs[i].contrast_noise_png_path);
        GET_STR(anim, "brightness_noise_field", out_cfgs[i].brightness_noise_png_path);
        out_cfgs[i].contrast_walk_spec = parse_walk_spec(cJSON_GetObjectItem(anim, "contrast_walk_spec"));
        out_cfgs[i].brightness_walk_spec = parse_walk_spec(cJSON_GetObjectItem(anim, "brightness_walk_spec"));
        out_cfgs[i].noise_mode = parse_anim_mode(anim);
        parse_fnl_state_with_defaults(fnl_state_obj, &out_cfgs[i].fnl);

        bool default_noise_mode = !(is_mode_string_valid(anim_mode_obj) || is_mode_string_valid(noise_mode_obj));
        bool default_fnl_state = !(fnl_state_obj && cJSON_IsObject(fnl_state_obj));
        if (default_noise_mode || default_fnl_state) {
            ESP_LOGI("rgb_anim_dynamic",
                     "Anim id=%d defaults applied (%s%s)",
                     out_cfgs[i].id,
                     default_noise_mode ? "anim_mode " : "",
                     default_fnl_state ? "fnl_state" : "");
        }
    }

    if (rewrite_performed) {
        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str) {
            if (write_text_file(path, json_str)) {
                ESP_LOGI("rgb_anim_dynamic", "Migrated %s: noise_mode -> anim_mode", path);
            }
            cJSON_free(json_str);
        }
    }

    cJSON_Delete(root);
    *out_count = count;
    return true;
}

static void merge_missing_by_id(rgb_anim_dynamic_config_t *dst, int *dst_count,
                                const rgb_anim_dynamic_config_t *src, int src_count)
{
    if (!dst || !dst_count || !src) return;
    for (int i = 0; i < src_count; ++i) {
        if (*dst_count >= RGB_DYNAMIC_MAX_ANIMS) return;
        if (find_config_index_by_id(dst, *dst_count, src[i].id) >= 0) continue;
        dst[*dst_count] = src[i];
        (*dst_count)++;
    }
}

static void parse_fnl_state_with_defaults(cJSON *fnl_obj, fnl_state *out)
{
    if (!out) return;

    *out = fnlCreateState();
    if (!fnl_obj || !cJSON_IsObject(fnl_obj)) {
        return;
    }

    #define FNL_SET_INT(field) do { \
        cJSON *it = cJSON_GetObjectItem(fnl_obj, #field); \
        if (it && cJSON_IsNumber(it)) out->field = it->valueint; \
    } while (0)

    #define FNL_SET_FLOAT(field) do { \
        cJSON *it = cJSON_GetObjectItem(fnl_obj, #field); \
        if (it && cJSON_IsNumber(it)) out->field = (float)it->valuedouble; \
    } while (0)

    FNL_SET_INT(seed);
    FNL_SET_FLOAT(frequency);
    FNL_SET_INT(noise_type);
    FNL_SET_INT(rotation_type_3d);
    FNL_SET_INT(fractal_type);
    FNL_SET_INT(octaves);
    FNL_SET_FLOAT(lacunarity);
    FNL_SET_FLOAT(gain);
    FNL_SET_FLOAT(weighted_strength);
    FNL_SET_FLOAT(ping_pong_strength);
    FNL_SET_INT(cellular_distance_func);
    FNL_SET_INT(cellular_return_type);
    FNL_SET_FLOAT(cellular_jitter_mod);
    FNL_SET_INT(domain_warp_type);
    FNL_SET_FLOAT(domain_warp_amp);

    #undef FNL_SET_FLOAT
    #undef FNL_SET_INT
}

static inline uint8_t rgb_value(rgb_color_t c) {
    uint8_t max = c.r > c.g ? c.r : c.g;
    return max > c.b ? max : c.b;
}

static inline float map_noise_to_unit(float n)
{
    float t = (n + 1.0f) * 0.5f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static rgb_color_t sample_palette_color(float t)
{
    rgb_color_t out = {0, 0, 0};
    if (!anim_buffers.palette_raw_rgb || anim_buffers.palette_raw_size < 3) {
        return out;
    }

    size_t pal_count = anim_buffers.palette_raw_size / 3;
    if (pal_count == 0) {
        return out;
    }

    size_t idx = (size_t)(t * (float)(pal_count - 1));
    if (idx >= pal_count) {
        idx = pal_count - 1;
    }

    const uint8_t *p = &anim_buffers.palette_raw_rgb[idx * 3];
    out.r = p[0];
    out.g = p[1];
    out.b = p[2];
    return out;
}

static uint8_t brightness_value_noise_rgb(rgb_color_t c, uint8_t noise, uint8_t user_brightness) {
    int16_t v = (int16_t)rgb_value(c) + (((int16_t)noise - 128) >> 5); // Small nudge +/- 8
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)(((uint16_t)v * user_brightness) >> 8);
}

void alloc_png_bufs(void) {
    bool ok = true;
    // Keep headroom so common palette sizes (e.g., 1x512 RGB = 1536B) avoid realloc.
    anim_buffers.palette_raw_size = 2048;
    ok &= alloc_buffer((void**)&anim_buffers.palette_raw_rgb, anim_buffers.palette_raw_size, "palette_raw_rgb");

    if (!ok) {
        free(anim_buffers.palette_raw_rgb);
        memset(&anim_buffers, 0, sizeof(anim_buffers));
        ESP_LOGE("rgb_anim_dynamic", "Failed to allocate palette buffer");
    }
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
        io_rgb_register_rgb_plugin(s_configs[i].id, &s_dynamic_anim);
    }

    alloc_png_bufs();

    // Create semaphore and task for PNG loading
    if (!s_load_png_sem) {
        s_load_png_sem = xSemaphoreCreateBinary();
    }
    if (s_load_png_task_handle == NULL) {
        xTaskCreate(Load_PNG_Task, "Load_PNG_Task", s_load_png_task_stack, NULL, 5, &s_load_png_task_handle);
    }
}

bool rgb_anim_dynamic_reload(void) {
    if (!buffer) {
        ESP_LOGE("rgb_anim_dynamic", "JSON buffer not allocated");
        return false;
    }
    int temp_count = 0;

    const char *primary_path = config_primary_path(s_config_source);
    const char *fallback1_path = config_fallback1_path(s_config_source);
    const char *fallback2_path = config_fallback2_path(s_config_source);

    s_config_count = 0;
    bool have_primary = load_configs_from_file(primary_path, s_configs, &s_config_count);
    bool have_fallback1 = fallback1_path ? load_configs_from_file(fallback1_path, s_reload_scratch, &temp_count) : false;
    if (have_fallback1) {
        merge_missing_by_id(s_configs, &s_config_count, s_reload_scratch, temp_count);
    }

    bool have_fallback2 = false;
    if (fallback2_path) {
        temp_count = 0;
        have_fallback2 = load_configs_from_file(fallback2_path, s_reload_scratch, &temp_count);
        if (have_fallback2) {
            merge_missing_by_id(s_configs, &s_config_count, s_reload_scratch, temp_count);
        }
    }

    if (!have_primary && !have_fallback1 && !have_fallback2) {
        ESP_LOGW("rgb_anim_dynamic", "No dynamic RGB config found in chain for source=%d", (int)s_config_source);
        s_config_count = 0;
        return false;
    }

    return s_config_count > 0;
}

void rgb_anim_dynamic_set_config_source(rgb_anim_dynamic_config_source_t source)
{
    s_config_source = source;
}

rgb_anim_dynamic_config_source_t rgb_anim_dynamic_get_config_source(void)
{
    return s_config_source;
}

int rgb_anim_dynamic_count(void) {
    return s_config_count;
}

// --- Plugin interface implementations ---
static void dynamic_begin(int idx) {
    if (s_config_count <= 0) {
        s_active_idx = 0;
        return;
    }

    // Set the active config index for this plugin instance
    s_active_idx = idx;
    for(int i = 0; i < s_config_count; i++) {
        if (s_configs[i].id == idx) {
            s_active_idx = i;
            break;
        }
    }

    if (s_active_idx < 0 || s_active_idx >= s_config_count) {
        s_active_idx = 0;
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
    if (s_config_count <= 0 || s_active_idx < 0 || s_active_idx >= s_config_count) {
        out_rgb->r = 0;
        out_rgb->g = 0;
        out_rgb->b = 0;
        return;
    }

    rgb_anim_dynamic_config_t *cfg = &s_configs[s_active_idx];
    if (!anim_buffers.palette_raw_rgb || anim_buffers.palette_raw_size < 3) {
        out_rgb->r = 0;
        out_rgb->g = 0;
        out_rgb->b = 0;
        return;
    }

    float n_color = fnlGetNoise2D(&cfg->fnl, (float)s_contrast_walk.x, (float)s_contrast_walk.y);
    float n_brightness = fnlGetNoise2D(&cfg->fnl, (float)s_brightness_walk.x + 53.0f, (float)s_brightness_walk.y - 91.0f);

    rgb_color_t contrast = sample_palette_color(map_noise_to_unit(n_color));
    uint8_t noise = (uint8_t)(map_noise_to_unit(n_brightness) * 255.0f);
    uint8_t brightness = brightness_value_noise_rgb(contrast, noise, s_user_brightness);

    io_rgb_set_anim_brightness(brightness);
    *out_rgb = contrast;

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
