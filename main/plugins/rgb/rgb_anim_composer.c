#include "rgb_anim_composer.h"

// Compose a palette/noise-based animation frame
void rgb_anim_composer_step(
    const rgb_anim_composer_config_t *config,
    const hsv_color_t *theme_hsv,
    hsv_color_t *out
) {
    // 1. Sample noise fields (if present)
    uint8_t noise_contrast = noise_palette_lookup(
        config->contrast_noise_field->data,
        config->contrast_noise_field->x,
        config->contrast_noise_field->y
    );
    uint8_t noise_gentle = noise_palette_lookup(
        config->gentle_noise_field->data,
        config->gentle_noise_field->x,
        config->gentle_noise_field->y
    );

    // 2. Advance walk indices (if walk spec present for each field)
    noise_walk_step(
        &((noise_field_t *)config->contrast_noise_field)->x,
        &((noise_field_t *)config->contrast_noise_field)->y,
        config->contrast_walk_spec
    );
    noise_walk_step(
        &((noise_field_t *)config->gentle_noise_field)->x,
        &((noise_field_t *)config->gentle_noise_field)->y,
        config->gentle_walk_spec
    );

    // 3. Palette lookup (use palette index strategy and full HSV shift)
    const hsv_color_t *palette = config->palette;
    uint8_t idx = config->palette_index_strategy(noise_contrast, noise_gentle, config->palette_index_modifier);
    hsv_color_t shift = theme_hsv ? *theme_hsv : (hsv_color_t){0, 0, 0};
    hsv_color_t base = hsv_palette_lookup(palette, idx, shift);


    // 4. Brightness strategy (global brightness only, never modulate V)
    uint8_t global_brightness = config->brightness_strategy(base, idx, noise_gentle, config->user_brightness);
    io_rgb_set_anim_brightness(global_brightness);

    // 5. Output
    *out = base;
}
