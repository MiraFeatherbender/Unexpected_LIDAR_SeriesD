// MCP23017 I2C GPIO expander component
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_types.h"

// Opaque handle
typedef struct mcp23017_t* mcp23017_handle_t;

// Initialization / IOCON-like settings exposed to user
typedef struct {
    // I2C settings
    i2c_port_t i2c_port;            // prefer using i2c_master_get_bus_handle(0) fallback
    int sda_gpio;            // optional if creating new bus
    int scl_gpio;            // optional if creating new bus
    int i2c_freq_hz;         // default 400000

    // Device addressing
    bool auto_discover;      // if true, scans 0x20..0x27; if false, uses `addresses` and `addr_count`
    uint8_t addresses[8];    // user-specified addresses (0x20..0x27)
    int addr_count;          // valid when auto_discover == false

    // IOCON defaults for all devices managed
    bool bank_16bit;         // true = BANK=1 (dual 8-bit address space), false = BANK=0 (paired registers, 16-bit mode). default true
    bool seqop;              // sequential operation: true = SEQOP disabled (byte mode for reads/writes), false = sequential enabled
    bool odr;                // open-drain INT output
    bool intpol;             // interrupt polarity: true = active-high, false = active-low
    bool mirror;             // mirror INTA/INTB (set according to bank per your rules)
    bool disslw;             // disable slew rate on SDA
} mcp23017_config_t;

// Public API
esp_err_t mcp23017_create(const mcp23017_config_t *cfg, mcp23017_handle_t *out_handle);
esp_err_t mcp23017_delete(mcp23017_handle_t handle);

// Read or write a single pin value. pin is 0..15 (PORTA = 8..15 when 16-bit mapping as requested)
esp_err_t mcp23017_pin_read(mcp23017_handle_t handle, int dev_idx, uint8_t pin, uint8_t *level);
esp_err_t mcp23017_pin_write(mcp23017_handle_t handle, int dev_idx, uint8_t pin, uint8_t level);

// Read/write full port(s)
esp_err_t mcp23017_read_gpio16(mcp23017_handle_t handle, int dev_idx, uint16_t *value);
esp_err_t mcp23017_write_gpio16(mcp23017_handle_t handle, int dev_idx, uint16_t value);
esp_err_t mcp23017_read_gpio8(mcp23017_handle_t handle, int dev_idx, int port /*0=A,1=B*/, uint8_t *value);
esp_err_t mcp23017_write_gpio8(mcp23017_handle_t handle, int dev_idx, int port /*0=A,1=B*/, uint8_t value);

// Interrupt registers and helpers
esp_err_t mcp23017_get_intf(mcp23017_handle_t handle, int dev_idx, uint16_t *intf_bits);
esp_err_t mcp23017_get_intcap(mcp23017_handle_t handle, int dev_idx, uint16_t *intcap_bits);
// Convert 16-bit bitmask to list of pin indices in user-provided buffer. returns ESP_OK and sets out_count
esp_err_t mcp23017_intmask_to_list(uint16_t mask, uint8_t *out_list, size_t *out_count);

// Device discovery: returns number of detected devices and fills addresses[] with 0..7 entries
esp_err_t mcp23017_discover_bus(int i2c_port, uint8_t *addresses, int *out_count);

// Utility: read/write register (low-level)
esp_err_t mcp23017_reg_read8(mcp23017_handle_t handle, int dev_idx, uint8_t reg_addr, uint8_t *value);
esp_err_t mcp23017_reg_write8(mcp23017_handle_t handle, int dev_idx, uint8_t reg_addr, uint8_t value);
