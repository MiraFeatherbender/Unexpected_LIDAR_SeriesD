#include "rgb_anim_dynamic.h"
#include "io_rgb.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
// Include your PNG loader headers here

#define RGB_ANIM_JSON_PATH "data/rgb_animations.json"

// --- Walk Spec ---
typedef struct {
    int8_t min_dx;
    int8_t max_dx;
    int8_t min_dy;
    int8_t max_dy;
} noise_walk_spec_t;

// Opaque config struct definition
struct rgb_anim_dynamic_config {
    int id; // Plugin enum value
    string color_palette_png_path;
    string contrast_noise_png_path;
    string gentle_noise_png_path;
    noise_walk_spec_t contrast_walk_spec;
    noise_walk_spec_t gentle_walk_spec;
    brightness_strategy_t brightness_strategy;
};

// Static array of loaded configs
#define MAX_DYNAMIC_ANIMS 8
static rgb_anim_dynamic_config_t s_configs[MAX_DYNAMIC_ANIMS];
static int s_config_count = 0;

// Forward declarations for plugin interface
static void dynamic_begin(void);
static void dynamic_step(hsv_color_t *out_hsv);
static void dynamic_set_color(hsv_color_t hsv);
static void dynamic_set_brightness(uint8_t b);

static hsv_anim_t s_dynamic_anim = {
    .begin = dynamic_begin,
    .step = dynamic_step,
    .set_color = dynamic_set_color,
    .set_brightness = dynamic_set_brightness,
};

// Active config index
static int s_active_idx = 0;

void rgb_anim_dynamic_init(void) {
    // TODO: Parse JSON, load PNGs, fill s_configs
    // For each loaded animation, register with io_rgb
    for (int i = 0; i < s_config_count; ++i) {
        io_rgb_register_plugin(s_configs[i].id, &s_dynamic_anim);
    }
}

bool rgb_anim_dynamic_reload(void) {
    // TODO: Re-parse JSON and reload PNGs
    return true;
}

int rgb_anim_dynamic_count(void) {
    return s_config_count;
}

// --- Plugin interface implementations ---
static void dynamic_begin(void) {
    // TODO: Load/activate the config for the selected animation
}

static void dynamic_step(hsv_color_t *out_hsv) {
    // TODO: Use s_configs[s_active_idx] to compute the next frame
}

static void dynamic_set_color(hsv_color_t hsv) {
    // TODO: Update color in active config
}

static void dynamic_set_brightness(uint8_t b) {
    // TODO: Update brightness in active config
}
