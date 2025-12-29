#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

static uint32_t fire_base_color = 0;
static uint8_t fire_brightness = 255;

static void fire_begin(void)
{
    // Reset fire animation state here later
}

static void fire_step(void)
{
    // Temporary: orange flicker placeholder
    ums3_set_pixel_brightness(fire_brightness);
    ums3_set_pixel_color(255, 80, 10);
}

static void fire_set_color(uint32_t rgb)
{
    fire_base_color = rgb;
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