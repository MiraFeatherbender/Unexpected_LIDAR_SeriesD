
#include "lidar_message_builder.h"
#include "lidar_protocol_cmd.h"

// Unified helper for all commands (simple and payload-based)
size_t lidar_build_cmd(uint8_t *out_buf, size_t buf_size, uint8_t cmd_code, const uint8_t *payload, size_t payload_len)
{
    if (!out_buf || buf_size < 2)
        return 0;

    if (payload_len == 0) {
        out_buf[0] = LIDAR_CMD_START_FLAG;
        out_buf[1] = cmd_code;
        return 2;
    }

    if (buf_size < (3 + payload_len))
        return 0;

    out_buf[0] = LIDAR_CMD_START_FLAG;
    out_buf[1] = cmd_code;
    out_buf[2] = (uint8_t)payload_len;
    for (size_t i = 0; i < payload_len; ++i) {
        out_buf[3 + i] = payload[i];
    }
    // Checksum: sum of all payload bytes, modulo 256
    uint8_t checksum = 0;
    for (size_t i = 0; i < payload_len; ++i) {
        checksum += payload[i];
    }
    out_buf[3 + payload_len] = checksum;
    return 4 + payload_len;
}

// ---- Simple 2-byte command builders ----

size_t lidar_build_stop_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_STOP, NULL, 0);
}

size_t lidar_build_reset_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_RESET, NULL, 0);
}

size_t lidar_build_scan_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_SCAN, NULL, 0);
}

size_t lidar_build_force_scan_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_FORCE_SCAN, NULL, 0);
}

size_t lidar_build_get_info_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_GET_INFO, NULL, 0);
}

size_t lidar_build_get_health_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_GET_HEALTH, NULL, 0);
}

size_t lidar_build_get_samplerate_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_GET_SAMPLERATE, NULL, 0);
}

// EXPRESS_SCAN builder (see protocol PDF Sec 5.2.8)
// mode: one of LIDAR_EXPRESS_SCAN_MODE_*
size_t lidar_build_express_scan_cmd(uint8_t *out_buf, size_t buf_size, uint8_t mode)
{
    // Payload: [working_mode (1)][reserved1 (1)][reserved2 (1)][reserved3 (1)][reserved4 (1)]
    uint8_t payload[5] = { mode, 0, 0, 0, 0 };
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_EXPRESS_SCAN, payload, sizeof(payload));
}

// EXPRESS_SCAN mode-specific builders (for registry use)
size_t lidar_build_express_scan_legacy_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_express_scan_cmd(out_buf, buf_size, LIDAR_EXPRESS_SCAN_MODE_LEGACY);
}

size_t lidar_build_express_scan_boost_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_express_scan_cmd(out_buf, buf_size, LIDAR_EXPRESS_SCAN_MODE_BOOST);
}

size_t lidar_build_express_scan_sensitivity_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_express_scan_cmd(out_buf, buf_size, LIDAR_EXPRESS_SCAN_MODE_SENSITIVITY);
}

size_t lidar_build_express_scan_stability_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_express_scan_cmd(out_buf, buf_size, LIDAR_EXPRESS_SCAN_MODE_STABILITY);
}

// GET_LIDAR_CONF builder (see protocol PDF Sec 5.2.10)
// type: 16-bit sub-command type (e.g., LIDAR_CONF_SCAN_MODE_COUNT)
size_t lidar_build_get_lidar_conf_cmd(uint8_t *out_buf, size_t buf_size, uint16_t type)
{
    uint8_t payload[4];
    payload[0] = (uint8_t)(type & 0xFF);      // type low byte
    payload[1] = (uint8_t)((type >> 8) & 0xFF); // type high byte
    payload[2] = 0; // reserved
    payload[3] = 0; // reserved
    return lidar_build_cmd(out_buf, buf_size, LIDAR_CMD_GET_LIDAR_CONF, payload, sizeof(payload));
}

// GET_LIDAR_CONF sub-command builders (for registry use)
size_t lidar_build_get_lidar_conf_scan_mode_count_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_get_lidar_conf_cmd(out_buf, buf_size, LIDAR_CONF_SCAN_MODE_COUNT);
}

size_t lidar_build_get_lidar_conf_scan_mode_sampletime_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_get_lidar_conf_cmd(out_buf, buf_size, LIDAR_CONF_SCAN_MODE_SAMPLETIME);
}

size_t lidar_build_get_lidar_conf_scan_mode_max_dist_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_get_lidar_conf_cmd(out_buf, buf_size, LIDAR_CONF_SCAN_MODE_MAX_DIST);
}

size_t lidar_build_get_lidar_conf_scan_mode_ans_type_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_get_lidar_conf_cmd(out_buf, buf_size, LIDAR_CONF_SCAN_MODE_ANS_TYPE);
}

size_t lidar_build_get_lidar_conf_scan_mode_name_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_get_lidar_conf_cmd(out_buf, buf_size, LIDAR_CONF_SCAN_MODE_NAME);
}

size_t lidar_build_get_lidar_conf_scan_mode_typical_cmd(uint8_t *out_buf, size_t buf_size)
{
    return lidar_build_get_lidar_conf_cmd(out_buf, buf_size, LIDAR_CONF_SCAN_MODE_TYPICAL);
}
