#include "rgb_anim.h"
#include "UMSeriesD_idf.h"

static uint32_t hb_color = 0;
static uint8_t hb_brightness = 255;
static uint16_t hb_phase = 0;
static uint8_t hb_speed = 3;   // default phase increment

static void breathe_begin(void)
{
    hb_phase = 0;
}

static void breathe_step(void)
{
    // Temporary: simple triangular pulse
    uint8_t intensity = (hb_phase < 128) ? hb_phase : (255 - hb_phase);
    hb_phase = (hb_phase + hb_speed) & 0xFF;

    // Scale intensity by peak brightness (0–255)
    uint16_t scaled = (intensity * hb_brightness) >> 8;

    uint8_t r = (scaled * ((hb_color >> 16) & 0xFF)) >> 8;
    uint8_t g = (scaled * ((hb_color >> 8) & 0xFF)) >> 8;
    uint8_t b = (scaled * (hb_color & 0xFF)) >> 8;

    ums3_set_pixel_brightness(255);   // full hardware brightness
    ums3_set_pixel_color(r, g, b);
}

static void breathe_set_color(uint32_t rgb)
{
    hb_color = rgb;
}

static void breathe_set_brightness(uint8_t b)
{
    hb_brightness = b;
}

static const rgb_anim_t breathe_plugin = {
    .begin = breathe_begin,
    .step = breathe_step,
    .set_color = breathe_set_color,
    .set_brightness = breathe_set_brightness,
};

void rgb_anim_breathe_init(void)
{
    io_rgb_register_plugin(RGB_PLUGIN_BREATHE, &breathe_plugin);
}