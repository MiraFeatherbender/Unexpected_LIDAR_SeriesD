#ifndef RGB_ANIM_H
#define RGB_ANIM_H

#include <stdint.h>

// ---------------------------------------------------------
// Plugin ID enum (mirrors dispatcher target enum pattern)
// ---------------------------------------------------------

typedef enum {
#define X_RGB_PLUGIN(name) RGB_PLUGIN##name,
#include "rgb_plugins.def"
#undef X_RGB_PLUGIN
    RGB_PLUGIN_MAX   // <-- always last, auto-sizes registry
} rgb_plugin_id_t;

static const char * const rgb_plugin_names[] = {
#define X_RGB_PLUGIN(name) "RGB_PLUGIN" #name,
#include "rgb_plugins.def"
#undef X_RGB_PLUGIN
};

typedef struct {
    #define X_FIELD_HSV(name, ctype, jtype, desc) ctype name;
    #define X_FIELD_B(name, ctype, jtype, desc);
    #include "io_rgb.def"
    #undef X_FIELD_B
    #undef X_FIELD_HSV
} hsv_color_t;

typedef struct {
    void (*begin)(void);                        // reset internal state
    void (*step)(hsv_color_t *out_hsv);         // run one animation frame, output HSV
    void (*set_color)(hsv_color_t hsv);         // every plugin receives color in
    #define X_FIELD_HSV(name, ctype, jtype, desc);
    #define X_FIELD_B(name, ctype, jtype, desc) void (*set_brightness)(ctype name);          // every plugin receives brightness
    #include "io_rgb.def"
    #undef X_FIELD_B
    #undef X_FIELD_HSV
} rgb_anim_t;

// ---------------------------------------------------------
// Global RGB animation control API
// Implemented in io_rgb.c
// ---------------------------------------------------------
void io_rgb_set_anim_brightness(uint8_t b);


// ---------------------------------------------------------
// Registration API (mirrors dispatcher_register_handler)
// Implemented in io_rgb.c
// ---------------------------------------------------------
void io_rgb_register_plugin(rgb_plugin_id_t id,
                            const rgb_anim_t *plugin);

#endif // RGB_ANIM_H