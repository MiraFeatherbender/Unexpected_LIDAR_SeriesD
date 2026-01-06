// lidar_message_builder.c
// Refactored: unified struct-based command builder registry for RPLIDAR

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "lidar_protocol_cmd.h"
#include "lidar_message_builder.h"

// Unified helper for all commands (simple and payload-based)
size_t lidar_build_cmd(uint8_t *out_buf, size_t buf_size, uint8_t cmd_code, const uint8_t *payload, size_t payload_len)
{
    if (!out_buf || buf_size < 2)
        return 0;

    // Simple commands: just [0xA5][CMD]
    if (!payload || payload_len == 0) {
        out_buf[0] = LIDAR_CMD_START_FLAG;
        out_buf[1] = cmd_code;
        return 2;
    }

    // Payload-based commands: [0xA5][CMD][PAYLOAD][CHK]
    if (buf_size < (2 + payload_len + 1))
        return 0;
    out_buf[0] = LIDAR_CMD_START_FLAG;
    out_buf[1] = cmd_code;
    for (size_t i = 0; i < payload_len; ++i) {
        out_buf[2 + i] = payload[i];
    }
    // Checksum: sum of all payload bytes, modulo 256
    uint8_t checksum = 0;
    for (size_t i = 0; i < payload_len; ++i) {
        checksum += payload[i];
    }
    out_buf[2 + payload_len] = checksum;
    return 3 + payload_len;
}

// Generic builder: uses code and static payload from entry
static size_t lidar_generic_builder(uint8_t *out_buf, size_t buf_size, const void *entry_ptr) {
    const lidar_cmd_entry_t *entry = (const lidar_cmd_entry_t *)entry_ptr;
    return lidar_build_cmd(out_buf, buf_size, entry->code, entry->payload, entry->payload_len);
}

// Example special builder for EXPRESS_SCAN (mode in payload[0])
static size_t lidar_express_scan_builder(uint8_t *out_buf, size_t buf_size, const void *entry_ptr) {
    const lidar_cmd_entry_t *entry = (const lidar_cmd_entry_t *)entry_ptr;
    uint8_t payload[5] = { entry->payload ? entry->payload[0] : 0, 0, 0, 0, 0 };
    return lidar_build_cmd(out_buf, buf_size, entry->code, payload, 5);
}

// Example special builder for GET_LIDAR_CONF (type in payload[0,1])
static size_t lidar_get_lidar_conf_builder(uint8_t *out_buf, size_t buf_size, const void *entry_ptr) {
    const lidar_cmd_entry_t *entry = (const lidar_cmd_entry_t *)entry_ptr;
    uint8_t payload[4] = { 0 };
    if (entry->payload_len >= 2 && entry->payload) {
        payload[0] = entry->payload[0];
        payload[1] = entry->payload[1];
    }
    // reserved bytes already zero
    return lidar_build_cmd(out_buf, buf_size, entry->code, payload, 4);
}

// Registry: order must match lidar_cmd_t enum
const lidar_cmd_entry_t lidar_cmd_table[] = {
    { LIDAR_CMD_IDX_STOP, "STOP", LIDAR_CMD_STOP, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_RESET, "RESET", LIDAR_CMD_RESET, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_SCAN, "SCAN", LIDAR_CMD_SCAN, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_EXPRESS_SCAN_LEGACY, "EXPRESS_SCAN_LEGACY", LIDAR_CMD_EXPRESS_SCAN, (const uint8_t[]){LIDAR_EXPRESS_SCAN_MODE_LEGACY}, 1, lidar_express_scan_builder },
    { LIDAR_CMD_IDX_EXPRESS_SCAN_BOOST, "EXPRESS_SCAN_BOOST", LIDAR_CMD_EXPRESS_SCAN, (const uint8_t[]){LIDAR_EXPRESS_SCAN_MODE_BOOST}, 1, lidar_express_scan_builder },
    { LIDAR_CMD_IDX_EXPRESS_SCAN_SENSITIVITY, "EXPRESS_SCAN_SENSITIVITY", LIDAR_CMD_EXPRESS_SCAN, (const uint8_t[]){LIDAR_EXPRESS_SCAN_MODE_SENSITIVITY}, 1, lidar_express_scan_builder },
    { LIDAR_CMD_IDX_EXPRESS_SCAN_STABILITY, "EXPRESS_SCAN_STABILITY", LIDAR_CMD_EXPRESS_SCAN, (const uint8_t[]){LIDAR_EXPRESS_SCAN_MODE_STABILITY}, 1, lidar_express_scan_builder },
    { LIDAR_CMD_IDX_FORCE_SCAN, "FORCE_SCAN", LIDAR_CMD_FORCE_SCAN, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_GET_INFO, "GET_INFO", LIDAR_CMD_GET_INFO, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_GET_HEALTH, "GET_HEALTH", LIDAR_CMD_GET_HEALTH, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_GET_SAMPLERATE, "GET_SAMPLERATE", LIDAR_CMD_GET_SAMPLERATE, NULL, 0, lidar_generic_builder },
    { LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_COUNT, "GET_LIDAR_CONF_SCAN_MODE_COUNT", LIDAR_CMD_GET_LIDAR_CONF, (const uint8_t[]){LIDAR_CONF_SCAN_MODE_COUNT, 0}, 2, lidar_get_lidar_conf_builder },
    { LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_SAMPLETIME, "GET_LIDAR_CONF_SCAN_MODE_SAMPLETIME", LIDAR_CMD_GET_LIDAR_CONF, (const uint8_t[]){LIDAR_CONF_SCAN_MODE_SAMPLETIME, 0}, 2, lidar_get_lidar_conf_builder },
    { LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_MAX_DIST, "GET_LIDAR_CONF_SCAN_MODE_MAX_DIST", LIDAR_CMD_GET_LIDAR_CONF, (const uint8_t[]){LIDAR_CONF_SCAN_MODE_MAX_DIST, 0}, 2, lidar_get_lidar_conf_builder },
    { LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_ANS_TYPE, "GET_LIDAR_CONF_SCAN_MODE_ANS_TYPE", LIDAR_CMD_GET_LIDAR_CONF, (const uint8_t[]){LIDAR_CONF_SCAN_MODE_ANS_TYPE, 0}, 2, lidar_get_lidar_conf_builder },
    { LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_NAME, "GET_LIDAR_CONF_SCAN_MODE_NAME", LIDAR_CMD_GET_LIDAR_CONF, (const uint8_t[]){LIDAR_CONF_SCAN_MODE_NAME, 0}, 2, lidar_get_lidar_conf_builder },
    { LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_TYPICAL, "GET_LIDAR_CONF_SCAN_MODE_TYPICAL", LIDAR_CMD_GET_LIDAR_CONF, (const uint8_t[]){LIDAR_CONF_SCAN_MODE_TYPICAL, 0}, 2, lidar_get_lidar_conf_builder },
};

const size_t lidar_cmd_table_count = sizeof(lidar_cmd_table) / sizeof(lidar_cmd_table[0]);

// Usage example:
// size_t len = lidar_cmd_table[cmd_enum].builder(out_buf, buf_size, &lidar_cmd_table[cmd_enum]);
