#ifndef LIDAR_MESSAGE_BUILDER_H
#define LIDAR_MESSAGE_BUILDER_H

#include <stddef.h>
#include <stdint.h>
#include "lidar_protocol_cmd.h"

// Unified builder function signature
typedef size_t (*lidar_builder_fn)(uint8_t *out_buf, size_t buf_size, const void *entry);

// Protocol-accurate builder for all commands (simple and payload-based)
size_t lidar_build_cmd(uint8_t *out_buf, size_t buf_size, uint8_t cmd_code, const uint8_t *payload, size_t payload_len);

// Command registry entry
typedef struct {
	lidar_cmd_idx_t cmd;           // Enum index for direct lookup
	const char *name;              // Human-readable name
	uint8_t code;                  // Protocol command code
	const uint8_t *payload;        // Static payload, or NULL
	size_t payload_len;            // Payload length
	lidar_builder_fn builder;      // Builder function pointer
} lidar_cmd_entry_t;


// The command table (registry)
extern const lidar_cmd_entry_t lidar_cmd_table[];
extern const size_t lidar_cmd_table_count;

// Helper: build a message by enum index (DRY, safe)
static inline size_t lidar_build_by_idx(uint8_t *out_buf, size_t buf_size, lidar_cmd_idx_t idx) {
	return lidar_cmd_table[idx].builder(out_buf, buf_size, &lidar_cmd_table[idx]);
}

#endif // LIDAR_MESSAGE_BUILDER_H
