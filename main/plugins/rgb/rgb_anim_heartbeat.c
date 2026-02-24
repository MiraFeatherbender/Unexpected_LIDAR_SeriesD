#include "rgb_anim.h"
#include "rgb_core.h"
#include "UMSeriesD_idf.h"
#include <math.h>
#include <stddef.h>

// Internal HSV state
static hsv_color_t heartbeat_hsv = {0, 0, 0};
static uint8_t heartbeat_brightness = 255;
static uint16_t heartbeat_phase = 0;
static uint8_t heartbeat_speed = 4;   // default phase increment
static uint8_t heartbeat_waveform[256] = {0};
static uint8_t *heartbeat_phase_ref = NULL;

// Prerecorded heartbeat waveform (0–255)
static const uint8_t heartbeat_ecg[256] = {48, 47, 45, 45, 45, 46, 48, 49, 49, 47, 45, 43, 43, 43, 44, 44, 
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

static void heartbeat_begin(uint8_t *phase_u8)
{
    heartbeat_phase_ref = phase_u8;
    if (heartbeat_phase_ref) {
        *heartbeat_phase_ref = 0;
        return;
    }
    heartbeat_phase = 0;
}

// Updated: step() outputs HSV via pointer
static void heartbeat_step(hsv_color_t *out_hsv)
{
    uint8_t phase = heartbeat_phase_ref ? *heartbeat_phase_ref : (uint8_t)heartbeat_phase;
    uint8_t intensity = heartbeat_waveform[phase];
    phase = (phase + heartbeat_speed) & 0xFF;
    if (heartbeat_phase_ref) {
        *heartbeat_phase_ref = phase;
    } else {
        heartbeat_phase = phase;
    }

    // Scale intensity by peak brightness (0–255)
    uint16_t scaled_v = (intensity * heartbeat_brightness) >> 8;

    // Output HSV: keep H/S from state, modulate V
    out_hsv->h = heartbeat_hsv.h;
    out_hsv->s = heartbeat_hsv.s;
    out_hsv->v = scaled_v;
}

// Updated: set_color receives HSV
static void heartbeat_set_color(hsv_color_t hsv)
{
    heartbeat_hsv = hsv;
}

static void heartbeat_set_brightness(uint8_t b)
{
    heartbeat_brightness = b;
}

static bool heartbeat_sample_hsv(const rgb_core_sample_in_t *in, hsv_color_t *out_hsv)
{
    if (!in || !out_hsv) {
        return false;
    }

    if (!in->in.hsv_phase.phase_u8) {
        return false;
    }

    const hsv_color_t base_hsv = in->in.hsv_phase.base_hsv;
    uint8_t phase = *in->in.hsv_phase.phase_u8;
    uint8_t intensity = heartbeat_waveform[phase];

    phase = (phase + heartbeat_speed) & 0xFF;
    *in->in.hsv_phase.phase_u8 = phase;

    uint16_t scaled_v = (intensity * in->brightness) >> 8;
    out_hsv->h = base_hsv.h;
    out_hsv->s = base_hsv.s;
    out_hsv->v = (uint8_t)scaled_v;
    return true;
}

static const hsv_anim_t heartbeat_plugin = {
    .begin = heartbeat_begin,
    .step = heartbeat_step,
    .set_color = heartbeat_set_color,
    .set_brightness = heartbeat_set_brightness,
};

static const hsv_anim_ex_t heartbeat_plugin_ex = {
    .begin_phase = heartbeat_begin,
    .set_color = heartbeat_set_color,
    .set_brightness = heartbeat_set_brightness,
    .sample_hsv = heartbeat_sample_hsv,
};

void rgb_anim_heartbeat_init(void)
{
    io_rgb_register_hsv_plugin_ex(RGB_PLUGIN_HEARTBEAT, &heartbeat_plugin_ex);
    io_rgb_register_plugin(RGB_PLUGIN_HEARTBEAT, &heartbeat_plugin);
    float gamma = 0.78f;

    // Precompute gamma-corrected waveform
    for (int i = 0; i < 256; i++) {
        float normalized = (float)heartbeat_ecg[i] / 255.0f;
        float corrected = powf(normalized, gamma);
        heartbeat_waveform[i] = (uint8_t)(corrected * 255.0f + 0.5f);
    }
}