#ifndef IO_BATTERY_H
#define IO_BATTERY_H

#include <stdint.h>

// Initialize battery monitoring subsystem
void io_battery_init(void);

// Commands that can be dispatched to the battery module via dispatcher messages
typedef enum {
    BATTERY_CMD_NONE = 0,
    BATTERY_CMD_PAUSE,            // Pause periodic updates
    BATTERY_CMD_RESUME,           // Resume periodic updates
    BATTERY_CMD_OVERRIDE_PLUGIN,  // Override current RGB plugin (payload: plugin id in data[1])
    BATTERY_CMD_CLEAR_OVERRIDE,   // Clear override
    BATTERY_CMD_SET_INTERVAL_MS,  // Set step interval (payload: uint32_t little-endian at data[1..4])
} battery_cmd_t;

#endif // IO_BATTERY_H