// MCP23017 v2 - friendly API: enums and register constants
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_types.h"

// Port enums
typedef enum {
    MCP_PORT_A = 0,
    MCP_PORT_B = 1,
} mcp23017_port_t;

// Operation mode (8-bit helpers vs 16-bit helpers)
typedef enum {
    MCP_MODE_8BIT = 0,
    MCP_MODE_16BIT = 1,
} mcp23017_mode_t;

// Pull-up enum
typedef enum {
    MCP_PULLUP_DISABLE = 0,
    MCP_PULLUP_ENABLE = 1,
} mcp23017_pull_t;

// Interrupt/shorthand modes
typedef enum {
    MCP_INT_ANYEDGE = 0,
    MCP_INT_POSEDGE = 1,
    MCP_INT_NEGEDGE = 2,
} mcp23017_int_mode_t;

// Interrupt polarity / output type
typedef enum {
    MCP_INT_ACTIVE_LOW = 0,
    MCP_INT_ACTIVE_HIGH = 1,
    MCP_INT_OPENDRAIN = 2,
} mcp23017_int_polarity_t;

// Convenience: operate on full 8-bit port
#define MCP_PORT_ALL ((uint8_t)0xFF)

// Register map (BANK=0 / sequential addressing)
#define MCP_REG_IODIRA   0x00
#define MCP_REG_IODIRB   0x01
#define MCP_REG_IPOLA    0x02
#define MCP_REG_IPOLB    0x03
#define MCP_REG_GPINTENA 0x04
#define MCP_REG_GPINTENB 0x05
#define MCP_REG_DEFVALA  0x06
#define MCP_REG_DEFVALB  0x07
#define MCP_REG_INTCONA  0x08
#define MCP_REG_INTCONB  0x09
#define MCP_REG_IOCON    0x0A
#define MCP_REG_GPPUA    0x0C
#define MCP_REG_GPPUB    0x0D
#define MCP_REG_INTFA    0x0E
#define MCP_REG_INTFB    0x0F
#define MCP_REG_INTCAPA  0x10
#define MCP_REG_INTCAPB  0x11
#define MCP_REG_GPIOA    0x12
#define MCP_REG_GPIOB    0x13
#define MCP_REG_OLATA    0x14
#define MCP_REG_OLATB    0x15

// Version helper
const char *mcp23017_v2_version(void);

// High-level 8-bit port helpers (v2)
// port: MCP_PORT_A or MCP_PORT_B
esp_err_t mcp23017_port_read(void *handle, int dev_idx, mcp23017_port_t port, uint8_t *value);
esp_err_t mcp23017_port_write(void *handle, int dev_idx, mcp23017_port_t port, uint8_t value);
esp_err_t mcp23017_port_masked_write(void *handle, int dev_idx, mcp23017_port_t port, uint8_t mask, uint8_t value);
esp_err_t mcp23017_port_set_dir(void *handle, int dev_idx, mcp23017_port_t port, uint8_t dir_mask);
esp_err_t mcp23017_port_set_pullup(void *handle, int dev_idx, mcp23017_port_t port, uint8_t mask, mcp23017_pull_t pull);

// 16-bit helpers that simply wrap two 8-bit operations (PORTA as high byte, PORTB as low byte)
esp_err_t mcp23017_v2_read_gpio16(void *handle, int dev_idx, uint16_t *value);
esp_err_t mcp23017_v2_write_gpio16(void *handle, int dev_idx, uint16_t value);

// Opaque v2 handle
typedef struct mcp23017_v2_t* mcp23017_v2_handle_t;

// Bundle returned by full-auto setup
typedef struct {
    mcp23017_v2_handle_t handle; // v2 handle (NULL on failure)
    void *bus;                   // opaque bus handle (i2c_master_bus_handle_t)
    uint8_t addresses[8];
    int addr_count;
    bool owns_bus;
    bool defaults_applied;
} mcp23017_v2_bundle_t;

// Full-auto setup: autodiscover existing I2C bus, autodiscover devices, attach them,
// optionally restore register defaults (apply_defaults=true). Returns a filled bundle on success.
// The only required argument is out_bundle (non-NULL).
esp_err_t mcp23017_v2_auto_setup(mcp23017_v2_bundle_t *out_bundle, bool apply_defaults);

// Low-level v2 reg helpers (operate on v2 handle)
esp_err_t mcp23017_v2_reg_read8(mcp23017_v2_handle_t h, int dev_idx, uint8_t reg_addr, uint8_t *value);
esp_err_t mcp23017_v2_reg_write8(mcp23017_v2_handle_t h, int dev_idx, uint8_t reg_addr, uint8_t value);

// Lifecycle
esp_err_t mcp23017_v2_delete(mcp23017_v2_handle_t h);

