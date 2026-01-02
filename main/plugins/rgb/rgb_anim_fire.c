#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include "noise_data.h"
#include "hsv_palette.h"
#include <stdlib.h>

// Internal HSV state
static hsv_color_t fire_hsv = {18, 220, 255}; // Example: orange (h=18, s=220, v=255)
static uint8_t fire_brightness = 255;
static uint8_t fire_noise_index_x = 0; // Animation frame index
static uint8_t fire_noise_index_y = 0;

static void fire_begin(void)
{
    // Reset fire animation state here later
    fire_noise_index_x = 0;
    fire_noise_index_y = 0;
}

// Updated: step() outputs HSV via pointer
static void fire_step(hsv_color_t *out_hsv)
{
    // Color noise
    noise_palette_t color_noise_type = NOISE_PALETTE_OPENSIMPLEX2;
    const uint8_t (*color_noise_table)[PGM_WIDTH] = get_noise_palette(color_noise_type);
    uint8_t color_noise_value = color_noise_table[fire_noise_index_y][fire_noise_index_x];

    // Brightness noise
    noise_palette_t brightness_noise_type = NOISE_PALETTE_PERLIN;
    const uint8_t (*brightness_noise_table)[PGM_WIDTH] = get_noise_palette(brightness_noise_type);
    uint8_t brightness_noise_value = brightness_noise_table[fire_noise_index_y][fire_noise_index_x];

    // Modulate V channel by noise and user brightness
    uint8_t adjusted_brightness = (uint8_t)((uint16_t)color_noise_value * brightness_noise_value / 255); // Dim down for fire effect

    // Output HSV: palette-based color + theme hue + modulated brightness
    const hsv_color_t *palette = get_hsv_palette(HSV_PALETTE_FIRE);
    hsv_color_t palette_color = palette[(uint8_t)(((uint16_t)color_noise_value * brightness_noise_value) / 255)]; // Index into palette
    // Apply theme hue from dispatcher
    uint8_t theme_hue = fire_hsv.h;
    palette_color.h = hsv_shift_hue(palette_color.h, theme_hue); // Shift hue towards fire theme

    // Apply adjusted brightness
    io_rgb_set_anim_brightness((uint8_t)(adjusted_brightness*0.6)); // Scale down overall brightness for fire effect

    *out_hsv = palette_color;

    fire_noise_index_x += (rand() % 1);
    fire_noise_index_y += (rand() % 3);
}

// Updated: set_color receives HSV
static void fire_set_color(hsv_color_t hsv)
{
    fire_hsv = hsv;
}

static void fire_set_brightness(uint8_t b)
{
    fire_brightness = b;
}

static const rgb_anim_t fire_plugin = {
    .begin = fire_begin,
    .step = fire_step,
    .set_color = fire_set_color,
    .set_brightness = fire_set_brightness,
};

void rgb_anim_fire_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_FIRE, &fire_plugin);
}