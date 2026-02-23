#ifndef RGB_COMMANDS_H
#define RGB_COMMANDS_H

// Simple command codes for RGB REST/dispatcher extension
// data[5] is interpreted as rgb_cmd_t when present.
typedef enum {
    RGB_CMD_NONE = 0,
    RGB_CMD_RELOAD = 1,
} rgb_cmd_t;

#endif // RGB_COMMANDS_H
