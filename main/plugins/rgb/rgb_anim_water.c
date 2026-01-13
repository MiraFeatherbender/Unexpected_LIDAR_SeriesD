#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include "noise_data.h"
#include "hsv_palette.h"
#include "rgb_anim_composer.h"
#include <stdlib.h>

// --- water animation config ---
static noise_field_t water_contrast_field = {
    .data = openSimplex2_data,
    .x = 0, .y = 0
};
static noise_field_t water_gentle_field = {
    .data = perlin_noise_data,
    .x = 0, .y = 0
};

static noise_walk_spec_t water_contrast_walk_spec = {
    .min_dx = 0, .max_dx = 1, .min_dy = 0, .max_dy = 1
};

static noise_walk_spec_t water_gentle_walk_spec = {
    .min_dx = 0, .max_dx = 6, .min_dy = 0, .max_dy = 3
};

static hsv_color_t water_theme_color = {84, 0, 0}; // Default orange

static rgb_anim_composer_config_t water_composer_config = {
    .palette = NULL, // Set in init
    .contrast_noise_field = &water_contrast_field,
    .gentle_noise_field = &water_gentle_field,
    .contrast_walk_spec = &water_contrast_walk_spec,
    .gentle_walk_spec = &water_gentle_walk_spec,
    .palette_index_strategy = palette_index_blend,
    .palette_index_modifier = 200, // favor gentle noise
    .brightness_strategy = brightness_value_noise, // Use value-based brightness with noise
    .user_brightness = 0 // assigned by set_brightness
};

// --- Animation plugin methods ---
static void water_begin(void) {
    water_contrast_field.x = 0; water_contrast_field.y = 0;
    water_gentle_field.x = 0; water_gentle_field.y = 0;
}

static void water_step(hsv_color_t *out_hsv) {
    rgb_anim_composer_step(&water_composer_config, &water_theme_color, out_hsv);
}

static void water_set_color(hsv_color_t hsv) {
    water_theme_color = hsv;
}

static void water_set_brightness(uint8_t b) {
    water_composer_config.user_brightness = b;
}

static const hsv_anim_t water_plugin = {
    .begin = water_begin,
    .step = water_step,
    .set_color = water_set_color,
    .set_brightness = water_set_brightness,
};

void rgb_anim_water_init(void) {
    water_composer_config.palette = get_hsv_palette(HSV_PALETTE_WATER);
    io_rgb_register_plugin(RGB_PLUGIN_WATER, &water_plugin);
}