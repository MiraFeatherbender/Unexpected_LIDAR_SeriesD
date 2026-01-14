#ifndef BATTERY_JSON_H
#define BATTERY_JSON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Struct matches io_battery's battery_rgb_tier_t
typedef struct {
    float min_voltage;
    bool vbus_required;
    uint8_t plugin;
    uint8_t h, s, v, brightness;
} battery_rgb_tier_t;

// API
const battery_rgb_tier_t *battery_json_get_tiers(size_t *num_tiers);
int battery_json_reload(void); // returns 0 on success, <0 on error

#endif // BATTERY_JSON_H
