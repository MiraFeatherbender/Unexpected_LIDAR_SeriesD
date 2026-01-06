#include "lidar_response_parser.h"
#include "lidar_protocol_rsp.h"
#include <string.h>
#include <stdio.h>

// --- Macro for fixed-layout parsing ---
#define LIDAR_PARSE_FIXED(payload, len, out_struct_ptr) \
    ((len) >= sizeof(*(out_struct_ptr)) ? (memcpy((out_struct_ptr), (payload), sizeof(*(out_struct_ptr))), true) : false)

// --- GET_INFO parser (now uses macro) ---
bool lidar_parse_info(const uint8_t *payload, size_t len, void *out_struct, const void *unused) {
    return LIDAR_PARSE_FIXED(payload, len, (lidar_info_response_t *)out_struct);
}

// --- Example formatter for GET_INFO (interprets firmware as 8.8 fixed-point) ---
void lidar_format_info(const void *out_struct, char *buf, size_t buf_len, const void *struct_info) {
    if (!out_struct || !buf || buf_len < 1) return;
    const lidar_info_response_t *info = (const lidar_info_response_t *)out_struct;
    uint16_t fw = (info->firmware_major << 8) | info->firmware_minor;
    snprintf(buf, buf_len, "Model: %u, FW: %u.%02u, HW: %u, Serial: %02X%02X%02X...",
        info->model, fw >> 8, fw & 0xFF, info->hardware,
        info->serial[0], info->serial[1], info->serial[2]);
}

// --- Parser registry (sparse, NULL for unused codes) ---

const lidar_response_parser_entry_t *lidar_response_parser_table[LIDAR_RESP_TYPE_MAX] = {0};


// GET_INFO entry
static const lidar_response_parser_entry_t get_info_entry = {
    LIDAR_RSP_TYPE_GET_INFO, "GET_INFO", lidar_parse_info, NULL, lidar_format_info
};


// Registry setup (call once at startup)
void lidar_response_parser_init(void) {
    memset((void*)lidar_response_parser_table, 0, sizeof(lidar_response_parser_table));
    lidar_response_parser_table[LIDAR_RSP_TYPE_GET_INFO] = &get_info_entry;
    // Add more as needed, using LIDAR_RSP_TYPE_* defines
}

const size_t lidar_response_parser_table_size = LIDAR_RESP_TYPE_MAX;
