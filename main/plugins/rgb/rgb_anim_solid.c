#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

static uint32_t solid_color = 0;
static uint8_t solid_brightness = 255;

static void solid_begin(void)
{
    ums3_set_pixel_brightness(solid_brightness);
    ums3_set_pixel_color32(solid_color);
}

static void solid_step(void)
{
    ums3_set_pixel_color32(solid_color);
}

static void solid_set_color(uint32_t rgb)
{
    solid_color = rgb;
}

static void solid_set_brightness(uint8_t b)
{
    solid_brightness = b;
    ums3_set_pixel_brightness(b);
}

static const rgb_anim_t solid_plugin = {
    .begin = solid_begin,
    .step = solid_step,
    .set_color = solid_set_color,
    .set_brightness = solid_set_brightness,
};

void rgb_anim_solid_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_SOLID, &solid_plugin);
}