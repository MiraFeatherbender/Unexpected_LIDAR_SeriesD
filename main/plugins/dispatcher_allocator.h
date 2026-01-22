#pragma once

#include <stdint.h>

typedef struct {
    double F;       // fraction for streaming pool
    int C;          // concurrency credit
    int payload_size;
    int min_entries;
    int max_entries;
} pool_config_t;

// Load config from /data/dispatcher_pool_config.json (if present)
int dispatcher_allocator_load_config(void);

// Initialize module; will attempt to load configuration
void dispatcher_allocator_init(void);

const pool_config_t *dispatcher_allocator_get_streaming_config(void);
const pool_config_t *dispatcher_allocator_get_control_config(void);
