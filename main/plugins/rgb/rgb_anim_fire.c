#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include "noise_data.h"
#include "hsv_palette.h"
#include "rgb_anim_composer.h"
#include <stdlib.h>

// --- Fire animation config ---
static noise_field_t fire_contrast_field = {
    .data = openSimplex2_data,
    .x = 0, .y = 0
};
static noise_field_t fire_gentle_field = {
    .data = perlin_noise_data,
    .x = 0, .y = 0
};
static noise_walk_spec_t fire_walk_spec = {
    .min_dx = 0, .max_dx = 1, .min_dy = 0, .max_dy = 3
};
static hsv_color_t fire_theme_color = {18, 220, 255}; // Default orange

static rgb_anim_composer_config_t fire_composer_config = {
    .palette = NULL, // Set in init
    .contrast_noise_field = &fire_contrast_field,
    .gentle_noise_field = &fire_gentle_field,
    .contrast_walk_spec = &fire_walk_spec,
    .gentle_walk_spec = &fire_walk_spec,
    .palette_index_strategy = palette_index_multiply,
    .palette_index_modifier = 0, // Not used for multiply
    .brightness_strategy = brightness_index, // Use standard index-based brightness
    .user_brightness = 0 // assigned by set_brightness
};

// --- Animation plugin methods ---
static void fire_begin(void) {
    fire_contrast_field.x = 0; fire_contrast_field.y = 0;
    fire_gentle_field.x = 0; fire_gentle_field.y = 0;
}

static void fire_step(hsv_color_t *out_hsv) {
    rgb_anim_composer_step(&fire_composer_config, &fire_theme_color, out_hsv);
}

static void fire_set_color(hsv_color_t hsv) {
    fire_theme_color = hsv;
}

static void fire_set_brightness(uint8_t b) {
    fire_composer_config.user_brightness = b;
}

static const rgb_anim_t fire_plugin = {
    .begin = fire_begin,
    .step = fire_step,
    .set_color = fire_set_color,
    .set_brightness = fire_set_brightness,
};

void rgb_anim_fire_init(void) {
    fire_composer_config.palette = get_hsv_palette(HSV_PALETTE_FIRE);
    io_rgb_register_plugin(RGB_PLUGIN_FIRE, &fire_plugin);
}