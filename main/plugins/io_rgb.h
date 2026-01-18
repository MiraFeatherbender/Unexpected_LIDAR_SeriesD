#ifndef IO_RGB_H
#define IO_RGB_H

#include "dispatcher.h"

// Simple command codes for REST command extension
typedef enum {
    RGB_CMD_NONE = 0,
    RGB_CMD_RELOAD = 1, // trigger rgb_anim_dynamic_reload()
} rgb_cmd_t;

// Initialize RGB subsystem and register dispatcher handler
void io_rgb_init(void);

#endif // IO_RGB_H