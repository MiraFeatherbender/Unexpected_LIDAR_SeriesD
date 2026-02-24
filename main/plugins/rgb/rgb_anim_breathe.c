#include "rgb_anim.h"
#include "rgb_core.h"
#include "UMSeriesD_idf.h"
#include <stddef.h>

// Internal HSV state
static hsv_color_t breathe_hsv = {0, 0, 0};
static uint8_t breathe_brightness = 255;
static uint16_t breathe_phase = 0;
static uint8_t breathe_speed = 3;   // default phase increment
static uint8_t *breathe_phase_ref = NULL;

static void breathe_begin(uint8_t *phase_u8)
{
    breathe_phase_ref = phase_u8;
    if (breathe_phase_ref) {
        *breathe_phase_ref = 0;
        return;
    }
    breathe_phase = 0;
}

// Updated: step() outputs HSV via pointer
static void breathe_step(hsv_color_t *out_hsv)
{
    uint8_t phase = breathe_phase_ref ? *breathe_phase_ref : (uint8_t)breathe_phase;

    // Simple triangular pulse for V
    uint8_t intensity = (phase < 128) ? phase : (255 - phase);
    phase = (phase + breathe_speed) & 0xFF;
    if (breathe_phase_ref) {
        *breathe_phase_ref = phase;
    } else {
        breathe_phase = phase;
    }

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

static bool breathe_sample_hsv(const rgb_core_sample_in_t *in, hsv_color_t *out_hsv)
{
    if (!in || !out_hsv) {
        return false;
    }

    if (!in->in.hsv_phase.phase_u8) {
        return false;
    }

    const hsv_color_t base_hsv = in->in.hsv_phase.base_hsv;
    uint8_t phase = *in->in.hsv_phase.phase_u8;

    uint8_t intensity = (phase < 128) ? phase : (255 - phase);
    phase = (phase + breathe_speed) & 0xFF;
    *in->in.hsv_phase.phase_u8 = phase;

    uint16_t scaled_v = (intensity * in->brightness) >> 8;

    out_hsv->h = base_hsv.h;
    out_hsv->s = base_hsv.s;
    out_hsv->v = (uint8_t)scaled_v;
    return true;
}

static const hsv_anim_t breathe_plugin = {
    .begin = breathe_begin,
    .step = breathe_step,
    .set_color = breathe_set_color,
    .set_brightness = breathe_set_brightness,
};

static const hsv_anim_ex_t breathe_plugin_ex = {
    .begin_phase = breathe_begin,
    .set_color = breathe_set_color,
    .set_brightness = breathe_set_brightness,
    .sample_hsv = breathe_sample_hsv,
};

void rgb_anim_breathe_init(void)
{
    io_rgb_register_hsv_plugin_ex(RGB_PLUGIN_BREATHE, &breathe_plugin_ex);
    io_rgb_register_plugin(RGB_PLUGIN_BREATHE, &breathe_plugin);
}