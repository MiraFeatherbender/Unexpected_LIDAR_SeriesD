#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

static uint32_t hb_color = 0;
static uint8_t hb_brightness = 255;
static uint16_t hb_phase = 0;

static void heartbeat_begin(void)
{
    hb_phase = 0;
}

static void heartbeat_step(void)
{
    // Temporary: simple triangular pulse
    uint8_t intensity = (hb_phase < 128) ? hb_phase : (255 - hb_phase);
    hb_phase = (hb_phase + 4) & 0xFF;

    uint8_t r = (intensity * ((hb_color >> 16) & 0xFF)) >> 8;
    uint8_t g = (intensity * ((hb_color >> 8) & 0xFF)) >> 8;
    uint8_t b = (intensity * (hb_color & 0xFF)) >> 8;

    ums3_set_pixel_brightness(hb_brightness);
    ums3_set_pixel_color(r, g, b);
}

static void heartbeat_set_color(uint32_t rgb)
{
    hb_color = rgb;
}

static void heartbeat_set_brightness(uint8_t b)
{
    hb_brightness = b;
}

static const rgb_anim_t heartbeat_plugin = {
    .begin = heartbeat_begin,
    .step = heartbeat_step,
    .set_color = heartbeat_set_color,
    .set_brightness = heartbeat_set_brightness,
};

void rgb_anim_heartbeat_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_HEARTBEAT, &heartbeat_plugin);
}