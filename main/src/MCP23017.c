// Minimal MCP23017 I2C driver implementing requested API.
#include "MCP23017.h"
#include <string.h>
#include <stdlib.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "mcp23017";

// Internal register enumeration (logical)
typedef enum {
    R_IODIR = 0,
    R_IPOL,
    R_GPINTEN,
    R_DEFVAL,
    R_INTCON,
    R_IOCON,
    R_GPPU,
    R_INTFA,
    R_INTCAP,
    R_GPIO,
    R_OLAT,
} reg_t;

// Device instance struct
struct mcp23017_t {
    mcp23017_config_t cfg;
    uint8_t detected_addrs[8];
    int detected_count;
    i2c_master_bus_handle_t bus;                // bus handle
    i2c_master_dev_handle_t dev_handles[8];     // device handles per detected device
    bool bus_created;
};

// Helper: compute register address depending on BANK and desired port
static uint8_t reg_addr_for(const mcp23017_config_t *cfg, reg_t reg, int port)
{
    // port: 0 = A, 1 = B
    if (cfg->bank_16bit) {
        // BANK = 1 mapping
        uint8_t baseA[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A};
        uint8_t baseB[] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A};
        return (port==0) ? baseA[reg] : baseB[reg];
    } else {
        // BANK = 0 mapping (paired)
        uint8_t pair_base[] = {0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14};
        // even = A, odd = B
        return pair_base[reg] + (port?1:0);
    }
}

// Using driver/i2c_master.h APIs: operations are performed on device handles

// Device discovery (simple: try to add device and read IOCON register)
esp_err_t mcp23017_discover_bus(int i2c_port, uint8_t *addresses, int *out_count)
{
    // Prefer existing bus handle if present
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_master_get_bus_handle(i2c_port, &bus);
    if (err != ESP_OK) {
        // cannot get existing bus; user code should create bus separately
        *out_count = 0;
        return ESP_OK;
    }

    int found = 0;
    for (uint8_t a = 0x20; a <= 0x27; ++a) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = a,
            .scl_speed_hz = 400000,
        };
        i2c_master_dev_handle_t dev = NULL;
        if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) == ESP_OK) {
            uint8_t reg = 0x05;
            uint8_t tmp;
            if (i2c_master_transmit_receive(dev, &reg, 1, &tmp, 1, pdMS_TO_TICKS(200)) == ESP_OK) {
                addresses[found++] = a;
            }
            // remove device since this is just probing
            i2c_master_bus_rm_device(dev);
        }
        if (found >= 8) break;
    }
    *out_count = found;
    return ESP_OK;
}

esp_err_t mcp23017_create(const mcp23017_config_t *cfg, mcp23017_handle_t *out_handle)
{
    if (!cfg || !out_handle) return ESP_ERR_INVALID_ARG;
    mcp23017_handle_t h = calloc(1, sizeof(struct mcp23017_t));
    if (!h) return ESP_ERR_NO_MEM;
    memcpy(&h->cfg, cfg, sizeof(h->cfg));
    // default values
    if (h->cfg.i2c_freq_hz == 0) h->cfg.i2c_freq_hz = 400000;
    if (h->cfg.i2c_port < 0) h->cfg.i2c_port = I2C_NUM_0;
    // Get or create bus handle
    h->bus = NULL;
    h->bus_created = false;
    esp_err_t err = i2c_master_get_bus_handle(h->cfg.i2c_port, &h->bus);
    if (err != ESP_OK) {
        // try to create a new master bus using provided pins
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = h->cfg.i2c_port,
            .scl_io_num = h->cfg.scl_gpio,
            .sda_io_num = h->cfg.sda_gpio,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true }
        };
        err = i2c_new_master_bus(&bus_cfg, &h->bus);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get or create I2C bus handle: %d", err);
            free(h);
            return err;
        }
        h->bus_created = true;
    }

    // simple discovery or use provided list
    if (h->cfg.auto_discover) {
        for (uint8_t a = 0x20; a <= 0x27 && h->detected_count < 8; ++a) {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = a,
                .scl_speed_hz = h->cfg.i2c_freq_hz,
            };
            i2c_master_dev_handle_t dev = NULL;
            if (i2c_master_bus_add_device(h->bus, &dev_cfg, &dev) == ESP_OK) {
                uint8_t reg = 0x05;
                uint8_t tmp;
                if (i2c_master_transmit_receive(dev, &reg, 1, &tmp, 1, pdMS_TO_TICKS(200)) == ESP_OK) {
                    h->detected_addrs[h->detected_count] = a;
                    h->dev_handles[h->detected_count] = dev;
                    h->detected_count++;
                } else {
                    // remove device if probe failed
                    i2c_master_bus_rm_device(dev);
                }
            }
        }
    } else {
        for (int i = 0; i < h->cfg.addr_count && i < 8; ++i) {
            uint8_t a = h->cfg.addresses[i];
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = a,
                .scl_speed_hz = h->cfg.i2c_freq_hz,
            };
            i2c_master_dev_handle_t dev = NULL;
            if (i2c_master_bus_add_device(h->bus, &dev_cfg, &dev) == ESP_OK) {
                uint8_t reg = 0x05;
                uint8_t tmp;
                if (i2c_master_transmit_receive(dev, &reg, 1, &tmp, 1, pdMS_TO_TICKS(200)) == ESP_OK) {
                    h->detected_addrs[h->detected_count] = a;
                    h->dev_handles[h->detected_count] = dev;
                    h->detected_count++;
                } else {
                    i2c_master_bus_rm_device(dev);
                }
            }
        }
    }

    *out_handle = h;
    ESP_LOGI(TAG, "mcp23017: detected %d device(s)", h->detected_count);
    return ESP_OK;
}

esp_err_t mcp23017_delete(mcp23017_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    // remove any added device handles
    for (int i = 0; i < handle->detected_count; ++i) {
        if (handle->dev_handles[i]) {
            i2c_master_bus_rm_device(handle->dev_handles[i]);
            handle->dev_handles[i] = NULL;
        }
    }
    // if we created the bus, delete it
    if (handle->bus_created && handle->bus) {
        i2c_del_master_bus(handle->bus);
    }
    free(handle);
    return ESP_OK;
}

// Low level reg helpers exposed
esp_err_t mcp23017_reg_read8(mcp23017_handle_t handle, int dev_idx, uint8_t reg_addr, uint8_t *value)
{
    if (!handle || dev_idx < 0 || dev_idx >= handle->detected_count) return ESP_ERR_INVALID_ARG;
    i2c_master_dev_handle_t dev = handle->dev_handles[dev_idx];
    if (!dev) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(dev, &reg_addr, 1, value, 1, pdMS_TO_TICKS(200));
}

esp_err_t mcp23017_reg_write8(mcp23017_handle_t handle, int dev_idx, uint8_t reg_addr, uint8_t value)
{
    if (!handle || dev_idx < 0 || dev_idx >= handle->detected_count) return ESP_ERR_INVALID_ARG;
    i2c_master_dev_handle_t dev = handle->dev_handles[dev_idx];
    if (!dev) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = { reg_addr, value };
    return i2c_master_transmit(dev, buf, 2, pdMS_TO_TICKS(200));
}

// Read 8-bit GPIO port
esp_err_t mcp23017_read_gpio8(mcp23017_handle_t handle, int dev_idx, int port, uint8_t *value)
{
    if (!handle || !value) return ESP_ERR_INVALID_ARG;
    uint8_t reg = reg_addr_for(&handle->cfg, R_GPIO, port);
    return mcp23017_reg_read8(handle, dev_idx, reg, value);
}

esp_err_t mcp23017_write_gpio8(mcp23017_handle_t handle, int dev_idx, int port, uint8_t value)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    uint8_t reg = reg_addr_for(&handle->cfg, R_OLAT, port);
    return mcp23017_reg_write8(handle, dev_idx, reg, value);
}

// Read 16-bit combined: PORTA upper byte, PORTB lower byte when requested
esp_err_t mcp23017_read_gpio16(mcp23017_handle_t handle, int dev_idx, uint16_t *value)
{
    if (!handle || !value) return ESP_ERR_INVALID_ARG;
    uint8_t a,b;
    esp_err_t r1 = mcp23017_read_gpio8(handle, dev_idx, 0, &a);
    esp_err_t r2 = mcp23017_read_gpio8(handle, dev_idx, 1, &b);
    if (r1 != ESP_OK) return r1;
    if (r2 != ESP_OK) return r2;
    // Per your request: in 16-bit mode PORTA is upper byte, PORTB lower
    *value = (((uint16_t)a) << 8) | b;
    return ESP_OK;
}

esp_err_t mcp23017_write_gpio16(mcp23017_handle_t handle, int dev_idx, uint16_t value)
{
    uint8_t a = (value >> 8) & 0xFF;
    uint8_t b = value & 0xFF;
    esp_err_t r1 = mcp23017_write_gpio8(handle, dev_idx, 0, a);
    if (r1 != ESP_OK) return r1;
    return mcp23017_write_gpio8(handle, dev_idx, 1, b);
}

// Read a pin: read IODIR to decide whether to read GPIO or OLAT
esp_err_t mcp23017_pin_read(mcp23017_handle_t handle, int dev_idx, uint8_t pin, uint8_t *level)
{
    if (!handle || !level || pin > 15) return ESP_ERR_INVALID_ARG;
    int port = (pin < 8) ? 0 : 1; // port 0=A (pins 0-7), port1=B (8-15)
    uint8_t bit = (pin < 8) ? pin : (pin - 8);
    uint8_t iodir;
    uint8_t reg_iodir = reg_addr_for(&handle->cfg, R_IODIR, port);
    esp_err_t r = mcp23017_reg_read8(handle, dev_idx, reg_iodir, &iodir);
    if (r != ESP_OK) return r;
    if ((iodir >> bit) & 0x1) {
        // input -> read GPIO
        uint8_t gpio;
        r = mcp23017_read_gpio8(handle, dev_idx, port, &gpio);
        if (r != ESP_OK) return r;
        *level = (gpio >> bit) & 0x1;
    } else {
        // output -> read OLAT
        uint8_t olat;
        uint8_t reg_olat = reg_addr_for(&handle->cfg, R_OLAT, port);
        r = mcp23017_reg_read8(handle, dev_idx, reg_olat, &olat);
        if (r != ESP_OK) return r;
        *level = (olat >> bit) & 0x1;
    }
    return ESP_OK;
}

esp_err_t mcp23017_pin_write(mcp23017_handle_t handle, int dev_idx, uint8_t pin, uint8_t level)
{
    if (!handle || pin > 15) return ESP_ERR_INVALID_ARG;
    int port = (pin < 8) ? 0 : 1;
    uint8_t bit = (pin < 8) ? pin : (pin - 8);
    uint8_t reg_olat = reg_addr_for(&handle->cfg, R_OLAT, port);
    uint8_t olat;
    esp_err_t r = mcp23017_reg_read8(handle, dev_idx, reg_olat, &olat);
    if (r != ESP_OK) return r;
    if (level) olat |= (1 << bit); else olat &= ~(1 << bit);
    return mcp23017_reg_write8(handle, dev_idx, reg_olat, olat);
}

esp_err_t mcp23017_get_intf(mcp23017_handle_t handle, int dev_idx, uint16_t *intf_bits)
{
    if (!handle || !intf_bits) return ESP_ERR_INVALID_ARG;
    uint8_t a,b;
    esp_err_t r1 = mcp23017_reg_read8(handle, dev_idx, reg_addr_for(&handle->cfg, R_INTFA, 0), &a);
    esp_err_t r2 = mcp23017_reg_read8(handle, dev_idx, reg_addr_for(&handle->cfg, R_INTFA, 1), &b);
    if (r1 != ESP_OK) return r1;
    if (r2 != ESP_OK) return r2;
    *intf_bits = (((uint16_t)a) << 8) | b;
    return ESP_OK;
}

esp_err_t mcp23017_get_intcap(mcp23017_handle_t handle, int dev_idx, uint16_t *intcap_bits)
{
    if (!handle || !intcap_bits) return ESP_ERR_INVALID_ARG;
    uint8_t a,b;
    esp_err_t r1 = mcp23017_reg_read8(handle, dev_idx, reg_addr_for(&handle->cfg, R_INTCAP, 0), &a);
    esp_err_t r2 = mcp23017_reg_read8(handle, dev_idx, reg_addr_for(&handle->cfg, R_INTCAP, 1), &b);
    if (r1 != ESP_OK) return r1;
    if (r2 != ESP_OK) return r2;
    *intcap_bits = (((uint16_t)a) << 8) | b;
    return ESP_OK;
}

esp_err_t mcp23017_intmask_to_list(uint16_t mask, uint8_t *out_list, size_t *out_count)
{
    if (!out_list || !out_count) return ESP_ERR_INVALID_ARG;
    size_t cnt = 0;
    for (int i = 0; i < 16; ++i) {
        if (mask & (1u << i)) {
            out_list[cnt++] = i;
        }
    }
    *out_count = cnt;
    return ESP_OK;
}
