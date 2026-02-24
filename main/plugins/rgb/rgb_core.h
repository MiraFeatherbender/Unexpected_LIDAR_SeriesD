#ifndef RGB_CORE_H
#define RGB_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "rgb_anim.h"

typedef struct {
    uint8_t plugin_id;
    hsv_color_t hsv;
    uint8_t brightness;
} rgb_core_snapshot_t;

void rgb_core_init(void);

void rgb_core_register_hsv_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin);
void rgb_core_register_rgb_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin);
void rgb_core_register_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin);

void rgb_core_set_phase_ptr(uint8_t *phase_u8);

void rgb_core_apply_command(uint8_t plugin_id, uint8_t h, uint8_t s, uint8_t v, uint8_t brightness);
bool rgb_core_step_rgb(rgb_color_t *out_rgb);

void rgb_core_get_snapshot(rgb_core_snapshot_t *snapshot);

#endif // RGB_CORE_H
