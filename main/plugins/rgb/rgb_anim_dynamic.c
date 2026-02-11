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
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define RGB_ANIM_JSON_PATH "/data/rgb_animations.json"
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
    *ptr = malloc(size);
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
};

static noise_walk_state_t s_contrast_walk = {0};
static noise_walk_state_t s_brightness_walk = {0};
static uint8_t s_user_brightness = 255;


// Static array of loaded configs
#define MAX_DYNAMIC_ANIMS 12
static rgb_anim_dynamic_config_t s_configs[MAX_DYNAMIC_ANIMS];
static int s_config_count = 0;

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
        uint8_t *new_buf = (uint8_t *)realloc(*out_buf, read_bytes);
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
                // Load source images (noise fields + palette)
                int bytes = read_image_file(s_configs[idx].contrast_noise_png_path, &anim_buffers.contrast_gray_init, &anim_buffers.contrast_gray_size, 1);
                int bytes2 = read_image_file(s_configs[idx].brightness_noise_png_path, &anim_buffers.brightness_gray_init, &anim_buffers.brightness_gray_size, 1);
                int bytes3 = read_image_file(s_configs[idx].color_palette_png_path, &anim_buffers.palette_raw_rgb, &anim_buffers.palette_raw_size, 3);

                // Apply palette to contrast grayscale to produce RGB noise field (and build linear palette)
                const uint8_t *gray = anim_buffers.contrast_gray_init;
                rgb_color_t *rgb = anim_buffers.contrast_rgb_colored;
                const uint8_t *pal = anim_buffers.palette_raw_rgb;

                // If we already have a linear palette buffer free it first
                if (anim_buffers.palette_linear) {
                    free(anim_buffers.palette_linear);
                    anim_buffers.palette_linear = NULL;
                }
                size_t pal_count = anim_buffers.palette_raw_size / 3;
                if (pal_count == 0) pal_count = 256; // fallback
                // allocate linear palette (float RGB values 0..1)
                anim_buffers.palette_linear = (float *)malloc(pal_count * 3 * sizeof(float));
                if (!anim_buffers.palette_linear) {
                    ESP_LOGE("rgb_anim_dynamic", "Failed to allocate linear palette");
                }

                // populate both the 8-bit colored buffer (for compatibility) and linear palette
                for (size_t i = 0; i < pal_count; ++i) {
                    if (pal && (i * 3 + 2) < anim_buffers.palette_raw_size) {
                        uint8_t pr = pal[i*3 + 0];
                        uint8_t pg = pal[i*3 + 1];
                        uint8_t pb = pal[i*3 + 2];
                        anim_buffers.palette_linear[i*3 + 0] = (pr <= 0) ? 0.0f : ( (pr / 255.0f) <= 0.04045f ? (pr / 255.0f) / 12.92f : powf(((pr / 255.0f) + 0.055f) / 1.055f, 2.4f) );
                        anim_buffers.palette_linear[i*3 + 1] = (pg <= 0) ? 0.0f : ( (pg / 255.0f) <= 0.04045f ? (pg / 255.0f) / 12.92f : powf(((pg / 255.0f) + 0.055f) / 1.055f, 2.4f) );
                        anim_buffers.palette_linear[i*3 + 2] = (pb <= 0) ? 0.0f : ( (pb / 255.0f) <= 0.04045f ? (pb / 255.0f) / 12.92f : powf(((pb / 255.0f) + 0.055f) / 1.055f, 2.4f) );
                    } else {
                        anim_buffers.palette_linear[i*3 + 0] = 0.0f;
                        anim_buffers.palette_linear[i*3 + 1] = 0.0f;
                        anim_buffers.palette_linear[i*3 + 2] = 0.0f;
                    }
                }

                // (previously yielded to watchdog; removed to restore original timing)

                // Build intermediate 8-bit colored (preserve original behavior)
                for (int i = 0; i < 256 * 256; ++i) {
                    uint16_t idx = *gray++;
                    idx = idx << 1; // 0-255 -> 0-510 (even indices)
                    const uint8_t *p = pal + (idx * 3);
                    rgb->r = p[0];
                    rgb->g = p[1];
                    rgb->b = p[2];
                    ++rgb;
                }

                // (removed brief sleep)

                // Blur contrast RGB channels in linear float space, scatter back to uint8 staging
                // For each channel: build float src from palette_linear mapped indices, blur in float, convert back
                const uint8_t *gray2 = anim_buffers.contrast_gray_init;
                // process R,G,B
                for (int ch = 0; ch < 3; ++ch) {
                    // fill channel_float_src with linear palette values
                    float *srcf = anim_buffers.channel_float_src;
                    for (int i = 0; i < 256 * 256; ++i) {
                        uint16_t idx = gray2[i];
                        idx = idx << 1;
                        size_t pal_idx = (size_t)idx % (pal_count);
                        srcf[i] = anim_buffers.palette_linear[pal_idx * 3 + ch];
                    }
                    // blur float channel and scatter converted sRGB back to uint8 staging
                    blur_channel_float_and_scatter(srcf, (uint8_t *)&anim_buffers.contrast_rgb_staging[0].r + ch, 256, 256, sizeof(rgb_color_t));
                }

                // Blur brightness field using linear float path (convert grayscale to linear float, blur, convert back)
                // Copy grayscale (0..255) into float linear (0..1) buffer
                float *b_srcf = anim_buffers.channel_float_src; // reuse temp buffer
                for (int i = 0; i < 256 * 256; ++i) {
                    // normalize to 0..1 and treat as linear luminance
                    b_srcf[i] = (float)anim_buffers.brightness_gray_init[i] / 255.0f;
                }
                // pad and blur into unpadded_blur_buf
                pad_image_f(b_srcf, anim_buffers.padded_blur_buf, 256, 256);
                blur_image_simd(anim_buffers.padded_blur_buf, BLUR_KERNEL_WEIGHTS, 256, 256);
                // scatter back center region into uint8 staging (linear->sRGB8 conversion)
                int padded_width = 256 + 2;
                for (int row = 0; row < 256; ++row) {
                    for (int col = 0; col < 256; ++col) {
                        int src_idx = (row + 1) * padded_width + (col + 1);
                        float lin = anim_buffers.unpadded_blur_buf[src_idx];
                        // linear (0..1) -> sRGB (0..1)
                        float s;
                        if (lin <= 0.0f) s = 0.0f;
                        else if (lin <= 0.0031308f) s = 12.92f * lin;
                        else s = 1.055f * powf(lin, 1.0f/2.4f) - 0.055f;
                        int v = (int)fminf(fmaxf(s * 255.0f, 0.0f), 255.0f);
                        anim_buffers.brightness_gray_staging[row * 256 + col] = (uint8_t)v;
                    }
                    	
                }

                // Promote staging buffers to active
                swap_ptrs((void**)&anim_buffers.contrast_rgb_active, (void**)&anim_buffers.contrast_rgb_staging);
                swap_ptrs((void**)&anim_buffers.brightness_gray_active, (void**)&anim_buffers.brightness_gray_staging);
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

static inline uint8_t rgb_value(rgb_color_t c) {
    uint8_t max = c.r > c.g ? c.r : c.g;
    return max > c.b ? max : c.b;
}

static uint8_t brightness_value_noise_rgb(rgb_color_t c, uint8_t noise, uint8_t user_brightness) {
    int16_t v = (int16_t)rgb_value(c) + (((int16_t)noise - 128) >> 5); // Small nudge +/- 8
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)(((uint16_t)v * user_brightness) >> 8);
}

void alloc_png_bufs(void) {
    // Allocate all necessary buffers for dynamic animation
    bool ok = true;
    anim_buffers.contrast_gray_size = 256*256;
    anim_buffers.brightness_gray_size = 256*256;
    anim_buffers.palette_raw_size = 256*3;
    ok &= alloc_buffer((void**)&anim_buffers.contrast_rgb_active,   sizeof(rgb_color_t)*256*256, "contrast_rgb_active");
    ok &= alloc_buffer((void**)&anim_buffers.contrast_rgb_staging,  sizeof(rgb_color_t)*256*256, "contrast_rgb_staging");
    ok &= alloc_buffer((void**)&anim_buffers.contrast_rgb_colored,  sizeof(rgb_color_t)*256*256, "contrast_rgb_colored");
    ok &= alloc_buffer((void**)&anim_buffers.contrast_gray_init,    anim_buffers.contrast_gray_size,                  "contrast_gray_init");
    ok &= alloc_buffer((void**)&anim_buffers.brightness_gray_active,256*256,                  "brightness_gray_active");
    ok &= alloc_buffer((void**)&anim_buffers.brightness_gray_staging,256*256,                 "brightness_gray_staging");
    ok &= alloc_buffer((void**)&anim_buffers.brightness_gray_init,  anim_buffers.brightness_gray_size,                  "brightness_gray_init");
    ok &= alloc_buffer((void**)&anim_buffers.palette_raw_rgb,       anim_buffers.palette_raw_size,                    "palette_raw_rgb");

    anim_buffers.padded_blur_buf = (float *)memalign(16, (256+2)*(256+2) * sizeof(float));
    anim_buffers.unpadded_blur_buf = (float *)memalign(16, (256+2)*(256+2) * sizeof(float));
    anim_buffers.channel_float_src = (float *)memalign(16, 256 * 256 * sizeof(float));

    if (!ok || !anim_buffers.padded_blur_buf || !anim_buffers.unpadded_blur_buf) {
        free(anim_buffers.contrast_rgb_active);
        free(anim_buffers.contrast_rgb_staging);
        free(anim_buffers.contrast_rgb_colored);
        free(anim_buffers.contrast_gray_init);
        free(anim_buffers.brightness_gray_active);
        free(anim_buffers.brightness_gray_staging);
        free(anim_buffers.brightness_gray_init);
        free(anim_buffers.palette_raw_rgb);
        free(anim_buffers.padded_blur_buf);
        free(anim_buffers.unpadded_blur_buf);
        free(anim_buffers.channel_float_src);
        
        // Reset pointers to NULL for safety
        memset(&anim_buffers, 0, sizeof(anim_buffers));

        ESP_LOGE("rgb_anim_dynamic", "Failed to allocate all PNG buffers");
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
        xTaskCreate(Load_PNG_Task, "Load_PNG_Task", 16384, NULL, 5, &s_load_png_task_handle);
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

// --- Plugin interface implementations ---
static void dynamic_begin(int idx) {
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
    if (!anim_buffers.contrast_rgb_active || !anim_buffers.brightness_gray_active) {
        out_rgb->r = 0;
        out_rgb->g = 0;
        out_rgb->b = 0;
        return;
    }

    uint32_t contrast_idx = ((uint32_t)s_contrast_walk.y << 8) | s_contrast_walk.x;
    uint32_t brightness_idx = ((uint32_t)s_brightness_walk.y << 8) | s_brightness_walk.x;
    rgb_color_t contrast = anim_buffers.contrast_rgb_active[contrast_idx];
    uint8_t noise = anim_buffers.brightness_gray_active[brightness_idx];
    // uint8_t brightness = 128;
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
