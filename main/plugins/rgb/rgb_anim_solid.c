#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

// Internal HSV state
static hsv_color_t solid_hsv = {0, 0, 0};
static uint8_t solid_brightness = 255;

static void solid_begin(void)
{
    // No hardware calls; just reset state if needed
}

static void solid_step(hsv_color_t *out_hsv)
{
    // Output the stored HSV color and brightness
    out_hsv->h = solid_hsv.h;
    out_hsv->s = solid_hsv.s;
    out_hsv->v = solid_brightness;
}

static void solid_set_color(hsv_color_t hsv)
{
    solid_hsv = hsv;
}

static void solid_set_brightness(uint8_t b)
{
    solid_brightness = b;
}

static const hsv_anim_t solid_plugin = {
    .begin = solid_begin,
    .step = solid_step,
    .set_color = solid_set_color,
    .set_brightness = solid_set_brightness,
};

void rgb_anim_solid_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_SOLID, &solid_plugin);
}