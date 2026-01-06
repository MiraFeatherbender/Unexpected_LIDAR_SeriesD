#ifndef LIDAR_RESPONSE_PARSER_H
#define LIDAR_RESPONSE_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LIDAR_RESP_TYPE_MAX 0x86 // Highest response type code + 1 (for array sizing)

// GET_INFO response struct (matches wire format)
typedef struct {
    uint8_t model;
    uint8_t firmware_minor; // low byte
    uint8_t firmware_major; // high byte
    uint8_t hardware;
    uint8_t serial[16];
} lidar_info_response_t;


// Unified parser function signature
// Accepts struct metadata for generic parsing
typedef bool (*lidar_response_parser_fn)(const uint8_t *payload, size_t len, void *out_struct, const void *struct_info);

// Optional: formatter function signature
typedef void (*lidar_response_formatter_fn)(const void *out_struct, char *buf, size_t buf_len, const void *struct_info);

// Descriptor for a received response
typedef struct {
    uint32_t payload_len;   // 24 bits from header, but use uint32_t for ease
    uint8_t response_type;
    const uint8_t *payload; // pointer to start of payload in the buffer
} lidar_response_desc_t;

// Registry entry for response parsers
typedef struct {
    uint8_t response_code; // or command idx, as needed
    const char *name;
    lidar_response_parser_fn parser;
    const void *struct_info; // struct layout, size, or type tag
    lidar_response_formatter_fn formatter; // optional
} lidar_response_parser_entry_t;



// Parser registry (extern)
extern const lidar_response_parser_entry_t *lidar_response_parser_table[LIDAR_RESP_TYPE_MAX];
extern const size_t lidar_response_parser_table_size;

// Initialization function
void lidar_response_parser_init(void);



// For fixed-layout responses, struct_info is unused (pass NULL). Parsing is handled by the LIDAR_PARSE_FIXED macro in the .c file.

// GET_INFO parser (can use generic or custom)
bool lidar_parse_info(const uint8_t *payload, size_t len, void *out_struct, const void *struct_info);

// Example formatter
void lidar_format_info(const void *out_struct, char *buf, size_t buf_len, const void *struct_info);

#endif // LIDAR_RESPONSE_PARSER_H
