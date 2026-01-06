// lidar_protocol_cmd.h
// Outgoing command constants for RPLIDAR protocol (see LR001_SLAMTEC_rplidar_protocol_v2.4_en.pdf)
// Only for use in message building (not response parsing)

#ifndef LIDAR_PROTOCOL_CMD_H
#define LIDAR_PROTOCOL_CMD_H


// Enum for all supported LIDAR protocol commands (for registry indexing)
typedef enum {
    LIDAR_CMD_IDX_STOP = 0,
    LIDAR_CMD_IDX_RESET,
    LIDAR_CMD_IDX_SCAN,
    LIDAR_CMD_IDX_EXPRESS_SCAN_LEGACY,
    LIDAR_CMD_IDX_EXPRESS_SCAN_BOOST,
    LIDAR_CMD_IDX_EXPRESS_SCAN_SENSITIVITY,
    LIDAR_CMD_IDX_EXPRESS_SCAN_STABILITY,
    LIDAR_CMD_IDX_FORCE_SCAN,
    LIDAR_CMD_IDX_GET_INFO,
    LIDAR_CMD_IDX_GET_HEALTH,
    LIDAR_CMD_IDX_GET_SAMPLERATE,
    LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_COUNT,
    LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_SAMPLETIME,
    LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_MAX_DIST,
    LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_ANS_TYPE,
    LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_NAME,
    LIDAR_CMD_IDX_GET_LIDAR_CONF_SCAN_MODE_TYPICAL,
    LIDAR_CMD_IDX_MOTOR_SPEED_CTRL,
    LIDAR_CMD_IDX_COUNT // Always last: number of supported commands
} lidar_cmd_idx_t;

// ---- Command Start Flag ----
#define LIDAR_CMD_START_FLAG 0xA5 // All commands start with this byte (PDF Sec 5.1)

// ---- Command Codes (PDF Sec 5.2) ----
#define LIDAR_CMD_STOP              0x25 // Stop scanning
#define LIDAR_CMD_RESET             0x40 // Reset core
#define LIDAR_CMD_SCAN              0x20 // Start legacy scan
#define LIDAR_CMD_EXPRESS_SCAN      0x82 // Start express scan (requires payload)
#define LIDAR_CMD_FORCE_SCAN        0x21 // Start scan regardless of speed
#define LIDAR_CMD_GET_INFO          0x50 // Get device info
#define LIDAR_CMD_GET_HEALTH        0x52 // Get device health
#define LIDAR_CMD_GET_SAMPLERATE    0x59 // Get sample rate
#define LIDAR_CMD_GET_LIDAR_CONF    0x84 // Get LIDAR configuration (requires payload)
#define LIDAR_CMD_MOTOR_SPEED_CTRL  0xA8 // Set motor speed (S1 only, requires payload)

// ---- EXPRESS_SCAN Payload: Working Modes (PDF Sec 5.2.8, Table 5-7) ----
#define LIDAR_EXPRESS_SCAN_MODE_LEGACY      0 // Legacy mode
#define LIDAR_EXPRESS_SCAN_MODE_BOOST       2 // Boost mode
#define LIDAR_EXPRESS_SCAN_MODE_SENSITIVITY 3 // Sensitivity mode
#define LIDAR_EXPRESS_SCAN_MODE_STABILITY   4 // Stability mode

// ---- GET_LIDAR_CONF Sub-Command Types (PDF Sec 5.2.10, Table 5-9) ----
#define LIDAR_CONF_SCAN_MODE_COUNT      0x70 // No payload, 16-bit response
#define LIDAR_CONF_SCAN_MODE_SAMPLETIME 0x71 // 16-bit payload, 32-bit response
#define LIDAR_CONF_SCAN_MODE_MAX_DIST   0x74 // 16-bit payload, 32-bit response
#define LIDAR_CONF_SCAN_MODE_ANS_TYPE   0x75 // 16-bit payload, 8-bit response
#define LIDAR_CONF_SCAN_MODE_NAME       0x7F // 16-bit payload, string response
#define LIDAR_CONF_SCAN_MODE_TYPICAL    0x7C // No payload, 16-bit response

#endif // LIDAR_PROTOCOL_CMD_H
