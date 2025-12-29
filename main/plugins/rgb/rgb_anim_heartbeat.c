#include "rgb_anim.h"
#include "UMSeriesD_idf.h"
#include <math.h>

static uint32_t hb_color = 0;
static uint8_t hb_brightness = 255;
static uint16_t hb_phase = 0;
static uint8_t hb_speed = 4;   // default phase increment
static uint8_t hb_waveform[256] = {0};

// Prerecorded heartbeat waveform (0–255)
static const uint8_t hb_ecg[256] = {48, 47, 45, 45, 45, 46, 48, 49, 49, 47, 45, 43, 43, 43, 44, 44, 
                                    44, 43, 41, 41, 40, 42, 44, 44, 44, 44, 42, 42, 42, 43, 45, 44, 
                                    43, 40, 36, 31, 28, 27, 26, 20, 12, 5, 3, 13, 36, 72, 115, 164, 
                                    210, 242, 255, 246, 218, 177, 128, 80, 41, 15, 2, 0, 2, 8, 16, 
                                    21, 25, 28, 29, 31, 33, 34, 36, 36, 35, 34, 33, 33, 33, 35, 36, 
                                    35, 35, 35, 34, 34, 35, 36, 38, 38, 38, 37, 36, 35, 36, 36, 38, 
                                    37, 37, 36, 35, 35, 35, 36, 38, 37, 37, 36, 34, 34, 34, 36, 38, 
                                    38, 38, 37, 35, 35, 36, 37, 39, 39, 39, 39, 38, 38, 39, 41, 43, 
                                    44, 44, 44, 43, 44, 45, 46, 47, 47, 47, 46, 45, 45, 45, 46, 48, 
                                    47, 46, 44, 42, 41, 41, 42, 43, 43, 42, 41, 40, 40, 41, 42, 44, 
                                    43, 43, 42, 40, 40, 41, 42, 43, 43, 42, 41, 39, 39, 40, 41, 43, 
                                    43, 43, 42, 40, 40, 40, 42, 43, 43, 42, 41, 40, 40, 40, 41, 42, 
                                    42, 42, 41, 41, 40, 41, 42, 43, 43, 42, 41, 39, 39, 40, 42, 43, 
                                    43, 42, 41, 40, 40, 40, 42, 43, 43, 42, 42, 41, 41, 42, 43, 44, 
                                    43, 42, 41, 40, 40, 41, 42, 43, 43, 42, 41, 41, 41, 43, 44, 46, 
                                    46, 46, 45, 45, 45, 46, 47, 49, 49, 50, 50, 50, 50, 50, 51, 51, 50};

static void heartbeat_begin(void)
{
    hb_phase = 0;
}

static void heartbeat_step(void)
{
    uint8_t intensity = hb_waveform[hb_phase];
    hb_phase = (hb_phase + hb_speed) & 0xFF;

    // Scale intensity by peak brightness (0–255)
    uint16_t scaled = (intensity * hb_brightness) >> 8;

    uint8_t r = (scaled * ((hb_color >> 16) & 0xFF)) >> 8;
    uint8_t g = (scaled * ((hb_color >> 8) & 0xFF)) >> 8;
    uint8_t b = (scaled * (hb_color & 0xFF)) >> 8;

    ums3_set_pixel_brightness(255);   // full hardware brightness
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
    float gamma = 0.78f;

    // Precompute gamma-corrected waveform
    for (int i = 0; i < 256; i++) {
        float normalized = (float)hb_ecg[i] / 255.0f;
        float corrected = powf(normalized, gamma);
        hb_waveform[i] = (uint8_t)(corrected * 255.0f + 0.5f);
    }
}