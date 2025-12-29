#ifndef RGB_ANIM_H
#define RGB_ANIM_H

#include <stdint.h>

// ---------------------------------------------------------
// Plugin ID enum (mirrors dispatcher target enum pattern)
// ---------------------------------------------------------
typedef enum {
    RGB_PLUGIN_OFF = 0,
    RGB_PLUGIN_SOLID,
    RGB_PLUGIN_FIRE,
    RGB_PLUGIN_HEARTBEAT,

    RGB_PLUGIN_MAX   // <-- always last, auto-sizes registry
} rgb_plugin_id_t;

// ---------------------------------------------------------
// Plugin interface (unified for ALL animations)
// ---------------------------------------------------------
typedef struct {
    void (*begin)(void);                // reset internal state
    void (*step)(void);                 // run one animation frame
    void (*set_color)(uint32_t rgb);    // every plugin receives color
    void (*set_brightness)(uint8_t b);  // every plugin receives brightness
} rgb_anim_t;

// ---------------------------------------------------------
// Registration API (mirrors dispatcher_register_handler)
// Implemented in io_rgb.c
// ---------------------------------------------------------
void io_rgb_register_plugin(rgb_plugin_id_t id,
                            const rgb_anim_t *plugin);

#endif // RGB_ANIM_H