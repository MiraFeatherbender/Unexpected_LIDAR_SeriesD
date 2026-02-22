#ifndef RGB_DYNAMIC_FNL_CONFIG_H
#define RGB_DYNAMIC_FNL_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "FastNoiseLite.h"
#include "rgb_anim.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RGB_DYNAMIC_FNL_SCHEMA_VERSION = 1,
    RGB_DYNAMIC_MAX_ANIMS = RGB_PLUGIN_MAX,
    RGB_DYNAMIC_NAME_MAX = 64,
    RGB_DYNAMIC_PALETTE_PATH_MAX = 128,
};

typedef enum {
    RGB_DYNAMIC_ANIM_MODE_DEDICATED = 0,
    RGB_DYNAMIC_ANIM_MODE_DYNAMIC = 1,
} rgb_dynamic_anim_mode_t;

typedef enum {
    RGB_DYNAMIC_BRIGHTNESS_STATIC = 0,
    RGB_DYNAMIC_BRIGHTNESS_VALUE_NOISE = 1,
} rgb_dynamic_brightness_strategy_t;

typedef struct {
    uint8_t id;
    char name[RGB_DYNAMIC_NAME_MAX];
    char palette_png_path[RGB_DYNAMIC_PALETTE_PATH_MAX];
    rgb_dynamic_anim_mode_t noise_mode;
    rgb_dynamic_brightness_strategy_t brightness_strategy;
    fnl_state fnl;
} rgb_dynamic_plugin_persist_t;

typedef struct {
    uint16_t schema_version;
    uint16_t anim_count;
    rgb_dynamic_plugin_persist_t anims[RGB_DYNAMIC_MAX_ANIMS];
} rgb_dynamic_file_config_t;

typedef struct {
    float offset_x;
    float offset_y;
    float offset_z;
    float z_delta;
    uint32_t frame_index;
} rgb_dynamic_walk_runtime_t;

typedef struct {
    rgb_dynamic_plugin_persist_t persisted;
    rgb_dynamic_walk_runtime_t runtime;
    bool active;
} rgb_dynamic_plugin_state_t;

#ifdef __cplusplus
}
#endif

#endif // RGB_DYNAMIC_FNL_CONFIG_H
