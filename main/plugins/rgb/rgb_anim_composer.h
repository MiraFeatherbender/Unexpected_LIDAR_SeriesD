#ifndef RGB_ANIM_COMPOSER_H
#define RGB_ANIM_COMPOSER_H

#include <stdint.h>
#include "rgb_anim.h"
#include "noise_data.h"
#include "hsv_palette.h"

// ---------------------------------------------
// Palette index strategy function pointer type
// ---------------------------------------------
typedef uint8_t (*rgb_palette_index_strategy_t)(uint8_t noise_contrast, uint8_t noise_gentle, uint8_t modifier);

// ---------------------------------------------
// Brightness strategy function pointer type
// ---------------------------------------------
typedef uint8_t (*rgb_brightness_strategy_t)(const hsv_color_t in, uint8_t noise_contrast, uint8_t noise_gentle, uint8_t user_brightness);

// ---------------------------------------------
// Composer config struct for palette/noise-based animations
// ---------------------------------------------
typedef struct {
    const hsv_color_t *palette;
    const noise_field_t *contrast_noise_field;
    const noise_field_t *gentle_noise_field;
    const noise_walk_spec_t *contrast_walk_spec;
    const noise_walk_spec_t *gentle_walk_spec;
    rgb_palette_index_strategy_t palette_index_strategy;
    uint8_t palette_index_modifier;
    rgb_brightness_strategy_t brightness_strategy;
    uint8_t user_brightness;
    // Future fields can be added here
} rgb_anim_composer_config_t;

// ---------------------------------------------
// Main composer step function
// ---------------------------------------------
void rgb_anim_composer_step(
    const rgb_anim_composer_config_t *config,
    const hsv_color_t *theme_hsv, // Provided by animation's set_color
    hsv_color_t *out
);

#endif // RGB_ANIM_COMPOSER_H
