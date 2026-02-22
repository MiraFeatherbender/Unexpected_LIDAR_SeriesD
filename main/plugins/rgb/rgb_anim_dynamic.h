#pragma once
#include "rgb_anim.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
	RGB_DYNAMIC_CONFIG_ONBOARD = 0,
	RGB_DYNAMIC_CONFIG_STRIP = 1,
} rgb_anim_dynamic_config_source_t;

// Initialize and register all JSON-based dynamic animations
void rgb_anim_dynamic_init(void);

// Optional: reload JSON config at runtime
bool rgb_anim_dynamic_reload(void);

// Select which config file chain to load (onboard or strip)
void rgb_anim_dynamic_set_config_source(rgb_anim_dynamic_config_source_t source);
rgb_anim_dynamic_config_source_t rgb_anim_dynamic_get_config_source(void);

// Internal: structure for loaded animation config (opaque to users)
typedef struct rgb_anim_dynamic_config rgb_anim_dynamic_config_t;

// For test/debug: get number of loaded animations
int rgb_anim_dynamic_count(void);

// For test/debug: get animation name by index
const char *rgb_anim_dynamic_name(int idx);

