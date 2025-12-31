#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include "openSimplex2_data.h"

// Internal HSV state
static hsv_color_t fire_hsv = {18, 220, 255}; // Example: orange (h=18, s=220, v=255)
static uint8_t fire_brightness = 255;
static uint16_t fire_noise_index = 0; // Animation frame index

static void fire_begin(void)
{
    // Reset fire animation state here later
    fire_noise_index = 0;
}

// Updated: step() outputs HSV via pointer
static void fire_step(hsv_color_t *out_hsv)
{
    // Use column 114 from OpenSimplex2 noise as brightness modulator
    uint8_t noise_brightness = pgm_data[fire_noise_index % PGM_HEIGHT][fire_noise_index % PGM_WIDTH];

    // Modulate V channel by noise and user brightness
    uint16_t mod_v = (noise_brightness * fire_brightness) / 255;

    // Output HSV: keep H/S from state, modulate V
    out_hsv->h = fire_hsv.h;
    out_hsv->s = fire_hsv.s;
    out_hsv->v = mod_v;

    fire_noise_index = (fire_noise_index + 1) % PGM_HEIGHT;
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