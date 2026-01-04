#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include "noise_data.h"
#include "hsv_palette.h"
#include "rgb_anim_composer.h"
#include <stdlib.h>

// --- aurora animation config ---
static noise_field_t aurora_contrast_field = {
    .data = openSimplex2_data,
    .x = 0, .y = 0
};
static noise_field_t aurora_gentle_field = {
    .data = perlin_noise_data,
    .x = 0, .y = 0
};

static noise_walk_spec_t aurora_contrast_walk_spec = {
    .min_dx = -1, .max_dx = 1, .min_dy = 0, .max_dy = 1
};

static noise_walk_spec_t aurora_gentle_walk_spec = {
    .min_dx = -1, .max_dx = 1, .min_dy = 0, .max_dy = 1
};

static hsv_color_t aurora_theme_color = {84, 0, 0}; // Default orange

static rgb_anim_composer_config_t aurora_composer_config = {
    .palette = NULL, // Set in init
    .contrast_noise_field = &aurora_contrast_field,
    .gentle_noise_field = &aurora_gentle_field,
    .contrast_walk_spec = &aurora_contrast_walk_spec,
    .gentle_walk_spec = &aurora_gentle_walk_spec,
    .palette_index_strategy = palette_index_blend,
    .palette_index_modifier = 255, // favor gentle noise
    .brightness_strategy = brightness_value_noise, // Use value-based brightness with noise
    .user_brightness = 0 // assigned by set_brightness
};

// --- Animation plugin methods ---
static void aurora_begin(void) {
    aurora_contrast_field.x = 0; aurora_contrast_field.y = 0;
    aurora_gentle_field.x = 0; aurora_gentle_field.y = 0;
}

static void aurora_step(hsv_color_t *out_hsv) {
    rgb_anim_composer_step(&aurora_composer_config, &aurora_theme_color, out_hsv);
}

static void aurora_set_color(hsv_color_t hsv) {
    aurora_theme_color = hsv;
}

static void aurora_set_brightness(uint8_t b) {
    aurora_composer_config.user_brightness = b;
}

static const rgb_anim_t aurora_plugin = {
    .begin = aurora_begin,
    .step = aurora_step,
    .set_color = aurora_set_color,
    .set_brightness = aurora_set_brightness,
};

void rgb_anim_aurora_init(void) {
    aurora_composer_config.palette = get_hsv_palette(HSV_PALETTE_AURORA);
    io_rgb_register_plugin(RGB_PLUGIN_AURORA, &aurora_plugin);
}