#ifndef RGB_CORE_H
#define RGB_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "rgb_anim.h"

typedef enum {
    RGB_CORE_IN_HSV_PHASE = 0,
    RGB_CORE_IN_NOISE_U8  = 1,
} rgb_core_input_mode_t;

typedef struct {
    rgb_core_input_mode_t mode;
    uint8_t plugin_id;
    uint8_t brightness;
    union {
        struct {
            hsv_color_t base_hsv;
            uint8_t *phase_u8;
        } hsv_phase;
        uint8_t noise_u8;
    } in;
} rgb_core_sample_in_t;

typedef struct {
    void (*begin_phase)(uint8_t *phase_u8);
    void (*set_color)(hsv_color_t hsv);
    void (*set_brightness)(uint8_t b);
    bool (*sample_hsv)(const rgb_core_sample_in_t *in, hsv_color_t *out_hsv);
} hsv_anim_ex_t;

typedef struct {
    void (*begin_phase)(uint8_t *phase_u8);
    void (*set_brightness)(uint8_t b);
    bool (*sample_rgb)(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);
} rgb_anim_ex_t;

typedef struct {
    uint8_t plugin_id;
    hsv_color_t hsv;
    uint8_t brightness;
} rgb_core_snapshot_t;

void rgb_core_init(void);

void rgb_core_register_hsv_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin);
void rgb_core_register_rgb_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin);
void rgb_core_register_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin);
void rgb_core_register_hsv_plugin_ex(rgb_plugin_id_t id, const hsv_anim_ex_t *plugin);
void rgb_core_register_rgb_plugin_ex(rgb_plugin_id_t id, const rgb_anim_ex_t *plugin);

void io_rgb_register_hsv_plugin_ex(rgb_plugin_id_t id, const hsv_anim_ex_t *plugin);
void io_rgb_register_rgb_plugin_ex(rgb_plugin_id_t id, const rgb_anim_ex_t *plugin);

void rgb_core_set_phase_ptr(uint8_t *phase_u8);

void rgb_core_apply_command(uint8_t plugin_id, uint8_t h, uint8_t s, uint8_t v, uint8_t brightness);
bool rgb_core_step_rgb(rgb_color_t *out_rgb);
bool rgb_core_sample(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);

void rgb_core_get_snapshot(rgb_core_snapshot_t *snapshot);

#endif // RGB_CORE_H
