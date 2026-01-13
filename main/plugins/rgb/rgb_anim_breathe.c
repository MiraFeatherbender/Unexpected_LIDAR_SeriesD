#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

// Internal HSV state
static hsv_color_t breathe_hsv = {0, 0, 0};
static uint8_t breathe_brightness = 255;
static uint16_t breathe_phase = 0;
static uint8_t breathe_speed = 3;   // default phase increment

static void breathe_begin(void)
{
    breathe_phase = 0;
}

// Updated: step() outputs HSV via pointer
static void breathe_step(hsv_color_t *out_hsv)
{
    // Simple triangular pulse for V
    uint8_t intensity = (breathe_phase < 128) ? breathe_phase : (255 - breathe_phase);
    breathe_phase = (breathe_phase + breathe_speed) & 0xFF;

    // Scale intensity by peak brightness (0–255)
    uint16_t scaled_v = (intensity * breathe_brightness) >> 8;

    // Output HSV: keep H/S from state, modulate V
    out_hsv->h = breathe_hsv.h;
    out_hsv->s = breathe_hsv.s;
    out_hsv->v = scaled_v;
}

// Updated: set_color receives HSV
static void breathe_set_color(hsv_color_t hsv)
{
    breathe_hsv = hsv;
}

static void breathe_set_brightness(uint8_t b)
{
    breathe_brightness = b;
}

static const hsv_anim_t breathe_plugin = {
    .begin = breathe_begin,
    .step = breathe_step,
    .set_color = breathe_set_color,
    .set_brightness = breathe_set_brightness,
};

void rgb_anim_breathe_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_BREATHE, &breathe_plugin);
}