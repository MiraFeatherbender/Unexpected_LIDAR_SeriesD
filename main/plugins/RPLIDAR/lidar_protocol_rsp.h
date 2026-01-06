// lidar_protocol_rsp.h
// Incoming response/packet constants for RPLIDAR protocol (see LR001_SLAMTEC_rplidar_protocol_v2.4_en.pdf)
// Only for use in response parsing (not command building)

#ifndef LIDAR_PROTOCOL_RSP_H
#define LIDAR_PROTOCOL_RSP_H

// ---- Response Descriptor Sync Bytes (PDF Sec 5.3.1) ----
#define LIDAR_RSP_SYNC_BYTE1 0xA5
#define LIDAR_RSP_SYNC_BYTE2 0x5A

// ---- Response Descriptor Send Mode Bits (PDF Sec 5.3.2) ----
#define LIDAR_RSP_SENDMODE_SINGLE_RESPONSE 0b00000000 // Single request - single response
#define LIDAR_RSP_SENDMODE_MULTI_RESPONSE  0b01000000 // Single request - multiple response
#define LIDAR_RSP_SENDMODE_BITS            0b11000000 // Mask for send mode bits in response length

// ---- Response Type Codes (PDF Sec 5.3.3, Table 5-11) ----
#define LIDAR_RSP_TYPE_SCAN_STANDARD      0x81 // Standard scan data
#define LIDAR_RSP_TYPE_SCAN_EXPRESS_LEGACY 0x82 // Express legacy scan data
#define LIDAR_RSP_TYPE_SCAN_EXPRESS_EXTEND 0x84 // Express extended scan data
#define LIDAR_RSP_TYPE_SCAN_EXPRESS_DENSE  0x85 // Express dense scan data
#define LIDAR_RSP_TYPE_GET_INFO           0x04 // Device info response
#define LIDAR_RSP_TYPE_GET_HEALTH         0x06 // Health response
#define LIDAR_RSP_TYPE_GET_SAMPLERATE     0x15 // Sample rate response
#define LIDAR_RSP_TYPE_GET_LIDAR_CONF     0x20 // LIDAR config response
#define LIDAR_RSP_TYPE_INVALID            0x00 // Not in PDF, used for error handling

// ---- Standard Data Packet Bit Masks (PDF Sec 5.4.1) ----
#define LIDAR_STD_DATA_ROT_START_BITS 0b00000011 // Rotation start flag(s)
#define LIDAR_STD_DATA_CHECK_BIT      0b00000001 // Check bit (should always be 1)

// ---- Express Data Packet Bit Masks (PDF Sec 5.4.2, 5.4.3) ----
#define LIDAR_EXPRESS_LEGACY_SYNC_BITS      0xF0 // Sync bits in express legacy packet
#define LIDAR_EXPRESS_LEGACY_ROT_START_BIT  0b10000000 // Rotation start flag in express legacy
#define LIDAR_EXPRESS_LEGACY_DIST_BITS      0b11111100 // Distance bits in express legacy
#define LIDAR_EXPRESS_EXTEND_MAJOR_BITS     0x0FFF     // Major value bits in express extended

// ---- Health Status Codes (PDF Sec 5.3.4) ----
#define LIDAR_HEALTH_STATUS_GOOD    0 // Status byte: good
#define LIDAR_HEALTH_STATUS_WARNING 1 // Status byte: warning
#define LIDAR_HEALTH_STATUS_ERROR   2 // Status byte: error

#endif // LIDAR_PROTOCOL_RSP_H
