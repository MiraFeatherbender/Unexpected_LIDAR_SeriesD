#include "lidar_message_builder.h"

// LIDAR protocol: CMD_GET_HEALTH = 0x52, CMD_START_FLAG = 0xA5
#define LIDAR_CMD_START_FLAG 0xA5
#define LIDAR_CMD_GET_HEALTH 0x52

size_t lidar_build_get_health_cmd(uint8_t *out_buf, size_t buf_size)
{
    if (!out_buf || buf_size < 2) return 0;
    out_buf[0] = LIDAR_CMD_START_FLAG;
    out_buf[1] = LIDAR_CMD_GET_HEALTH;
    return 2;
}
