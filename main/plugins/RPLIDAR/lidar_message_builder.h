#ifndef LIDAR_MESSAGE_BUILDER_H
#define LIDAR_MESSAGE_BUILDER_H

#include <stddef.h>
#include <stdint.h>

// Build a basic LIDAR Get Health command message
// Returns the length of the message (should be 2)
size_t lidar_build_get_health_cmd(uint8_t *out_buf, size_t buf_size);

#endif // LIDAR_MESSAGE_BUILDER_H
