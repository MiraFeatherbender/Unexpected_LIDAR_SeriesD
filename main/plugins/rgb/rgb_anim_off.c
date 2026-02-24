#include "rgb_anim.h"
#include "rgb_core.h"
#include "UMSeriesD_idf.h"

// Internal HSV state (kept for consistency)
static hsv_color_t off_hsv = {0, 0, 0};
static uint8_t off_brightness = 0;

static void off_begin(uint8_t *phase_u8)
{
    if (phase_u8) {
        *phase_u8 = 0;
    }
    // No hardware calls; just reset state if needed
}

static void off_step(hsv_color_t *out_hsv)
{
    // Always output black (V = 0)
    out_hsv->h = off_hsv.h; // hue is irrelevant
    out_hsv->s = off_hsv.s; // saturation is irrelevant
    out_hsv->v = 0;
}

static void off_set_color(hsv_color_t hsv)
{
    off_hsv = hsv; // ignored but stored
}

static void off_set_brightness(uint8_t b)
{
    off_brightness = b; // ignored but stored
}

static bool off_sample_hsv(const rgb_core_sample_in_t *in, hsv_color_t *out_hsv)
{
    if (!in || !out_hsv) {
        return false;
    }

    out_hsv->h = in->in.hsv_phase.base_hsv.h;
    out_hsv->s = in->in.hsv_phase.base_hsv.s;
    out_hsv->v = 0;
    return true;
}

static const hsv_anim_t off_plugin = {
    .begin = off_begin,
    .step = off_step,
    .set_color = off_set_color,
    .set_brightness = off_set_brightness,
};

static const hsv_anim_ex_t off_plugin_ex = {
    .begin_phase = off_begin,
    .set_color = off_set_color,
    .set_brightness = off_set_brightness,
    .sample_hsv = off_sample_hsv,
};

void rgb_anim_off_init(void)
{
    io_rgb_register_hsv_plugin_ex(RGB_PLUGIN_OFF, &off_plugin_ex);
    io_rgb_register_plugin(RGB_PLUGIN_OFF, &off_plugin);
}