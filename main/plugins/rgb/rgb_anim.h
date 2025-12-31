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
    RGB_PLUGIN_BREATHE,
    
    RGB_PLUGIN_MAX   // <-- always last, auto-sizes registry
} rgb_plugin_id_t;

// ---------------------------------------------------------
// HSV color struct (0-255 range for each channel)
// ---------------------------------------------------------
typedef struct {
    uint8_t h; // Hue (0-255)
    uint8_t s; // Saturation (0-255)
    uint8_t v; // Value/Brightness (0-255)
} hsv_color_t;

// ---------------------------------------------------------
// Plugin interface (unified for ALL animations)
// Updated: step() receives pointer to HSV output
// ---------------------------------------------------------
typedef struct {
    void (*begin)(void);                        // reset internal state
    void (*step)(hsv_color_t *out_hsv);         // run one animation frame, output HSV
    void (*set_color)(hsv_color_t hsv);         // every plugin receives color in HSV
    void (*set_brightness)(uint8_t b);          // every plugin receives brightness
} rgb_anim_t;

// ---------------------------------------------------------
// Registration API (mirrors dispatcher_register_handler)
// Implemented in io_rgb.c
// ---------------------------------------------------------
void io_rgb_register_plugin(rgb_plugin_id_t id,
                            const rgb_anim_t *plugin);

#endif // RGB_ANIM_H