#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

// Internal state (kept for consistency)
static uint32_t off_color = 0;
static uint8_t off_brightness = 0;

static void off_begin(void)
{
    // Always output black
    ums3_set_pixel_color(0, 0, 0);
}

static void off_step(void)
{
    // Always black
    ums3_set_pixel_color(0, 0, 0);
}

static void off_set_color(uint32_t rgb)
{
    off_color = rgb; // ignored but stored
}

static void off_set_brightness(uint8_t b)
{
    off_brightness = b; // ignored but stored
}

static const rgb_anim_t off_plugin = {
    .begin = off_begin,
    .step = off_step,
    .set_color = off_set_color,
    .set_brightness = off_set_brightness,
};

void rgb_anim_off_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_OFF, &off_plugin);
}