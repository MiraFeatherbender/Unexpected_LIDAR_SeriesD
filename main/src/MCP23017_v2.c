// Minimal MCP23017 v2 scaffold implementation
#include "MCP23017_v2.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mcp23017_v2";

const char *mcp23017_v2_version(void)
{
    return "mcp23017 v2 alpha";
}

// v2 internal device struct
struct mcp23017_v2_t {
    void *bus; // i2c_master_bus_handle_t
    void *dev_handles[8]; // i2c_master_dev_handle_t
    uint8_t addresses[8];
    int addr_count;
    bool owns_bus;
    // defaults flag
    bool defaults_applied;
};

static const uint8_t DEFAULT_REGS[] = {
    // 0x00..0x15 sequential defaults (IODIRA..OLATB)
    0xFF, // IODIRA
    0xFF, // IODIRB
    0x00, // IPOLA
    0x00, // IPOLB
    0x00, // GPINTENA
    0x00, // GPINTENB
    0x00, // DEFVALA
    0x00, // DEFVALB
    0x00, // INTCONA
    0x00, // INTCONB
    0x00, // IOCON (safe default)
    0x00, // (0x0B) IOCON alias / reserved
    0x00, // GPPUA
    0x00, // GPPUB
    0x00, // INTFA (read-only; write ignored)
    0x00, // INTFB (read-only; write ignored)
    0x00, // INTCAPA (read-only)
    0x00, // INTCAPB (read-only)
    0x00, // GPIOA
    0x00, // GPIOB
    0x00, // OLATA
    0x00, // OLATB
};

// Internal low-level helpers using i2c_master APIs
static esp_err_t v2_read8_device(void *dev, uint8_t reg, uint8_t *out)
{
    if (!dev || !out) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit_receive((i2c_master_dev_handle_t)dev, &reg, 1, out, 1, pdMS_TO_TICKS(200));
}

static esp_err_t v2_write8_device(void *dev, uint8_t reg, uint8_t val)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit((i2c_master_dev_handle_t)dev, buf, 2, pdMS_TO_TICKS(200));
}

// Create a v2 handle from an existing bus and a list of addresses (already attached)
static mcp23017_v2_handle_t v2_handle_create_from_devices(void *bus, uint8_t *addrs, i2c_master_dev_handle_t *devs, int count, bool owns_bus)
{
    mcp23017_v2_handle_t h = calloc(1, sizeof(struct mcp23017_v2_t));
    if (!h) return NULL;
    h->bus = bus;
    h->addr_count = count;
    h->owns_bus = owns_bus;
    for (int i = 0; i < count; ++i) {
        h->addresses[i] = addrs[i];
        h->dev_handles[i] = devs[i];
    }
    h->defaults_applied = false;
    return h;
}

esp_err_t mcp23017_v2_reg_read8(mcp23017_v2_handle_t h, int dev_idx, uint8_t reg_addr, uint8_t *value)
{
    if (!h || dev_idx < 0 || dev_idx >= h->addr_count) return ESP_ERR_INVALID_ARG;
    return v2_read8_device(h->dev_handles[dev_idx], reg_addr, value);
}

esp_err_t mcp23017_v2_reg_write8(mcp23017_v2_handle_t h, int dev_idx, uint8_t reg_addr, uint8_t value)
{
    if (!h || dev_idx < 0 || dev_idx >= h->addr_count) return ESP_ERR_INVALID_ARG;
    return v2_write8_device(h->dev_handles[dev_idx], reg_addr, value);
}

// Helper to attach a device at address 'a' to bus and return dev handle (or NULL)
static i2c_master_dev_handle_t v2_attach_device(void *bus, uint8_t a, int freq_hz)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = a,
        .scl_speed_hz = freq_hz > 0 ? freq_hz : 400000,
    };
    i2c_master_dev_handle_t dev = NULL;
    if (i2c_master_bus_add_device((i2c_master_bus_handle_t)bus, &dev_cfg, &dev) == ESP_OK) {
        return dev;
    }
    return NULL;
}

// Discover devices on an existing bus: try addresses 0x20..0x27 and attach devices that respond
static esp_err_t v2_discover_devices_on_bus(void *bus, uint8_t *out_addrs, i2c_master_dev_handle_t *out_devs, int *out_count)
{
    if (!bus || !out_addrs || !out_devs || !out_count) return ESP_ERR_INVALID_ARG;
    int found = 0;
    for (uint8_t a = 0x20; a <= 0x27 && found < 8; ++a) {
        i2c_master_dev_handle_t dev = v2_attach_device(bus, a, 400000);
        if (!dev) continue;
        // quick probe: read IOCON (0x0A) to confirm device presence
        uint8_t reg = 0x0A;
        uint8_t tmp;
        if (i2c_master_transmit_receive(dev, &reg, 1, &tmp, 1, pdMS_TO_TICKS(200)) == ESP_OK) {
            out_addrs[found] = a;
            out_devs[found] = dev;
            found++;
        } else {
            i2c_master_bus_rm_device(dev);
        }
    }
    *out_count = found;
    return ESP_OK;
}

// Apply datasheet defaults quickly by writing sequential block starting at 0x00 (IODIRA)
static esp_err_t v2_apply_defaults_to_device(i2c_master_dev_handle_t dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    const uint8_t start_reg = 0x00;
    const size_t n = sizeof(DEFAULT_REGS);
    uint8_t *buf = malloc(n + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    buf[0] = start_reg;
    memcpy(buf + 1, DEFAULT_REGS, n);
    esp_err_t rc = i2c_master_transmit(dev, buf, n + 1, pdMS_TO_TICKS(500));
    free(buf);
    return rc;
}

// delete v2 handle
esp_err_t mcp23017_v2_delete(mcp23017_v2_handle_t h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < h->addr_count; ++i) {
        if (h->dev_handles[i]) i2c_master_bus_rm_device((i2c_master_dev_handle_t)h->dev_handles[i]);
    }
    if (h->owns_bus && h->bus) i2c_del_master_bus((i2c_master_bus_handle_t)h->bus);
    free(h);
    return ESP_OK;
}

// Updated v2 high-level helpers that operate on v2 handle
esp_err_t mcp23017_port_read(void *handle, int dev_idx, mcp23017_port_t port, uint8_t *value)
{
    if (!handle || !value) return ESP_ERR_INVALID_ARG;
    return mcp23017_v2_reg_read8((mcp23017_v2_handle_t)handle, dev_idx, (port==MCP_PORT_A)?MCP_REG_GPIOA:MCP_REG_GPIOB, value);
}

esp_err_t mcp23017_port_write(void *handle, int dev_idx, mcp23017_port_t port, uint8_t value)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    return mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, (port==MCP_PORT_A)?MCP_REG_OLATA:MCP_REG_OLATB, value);
}

esp_err_t mcp23017_port_masked_write(void *handle, int dev_idx, mcp23017_port_t port, uint8_t mask, uint8_t value)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    uint8_t reg = (port==MCP_PORT_A)?MCP_REG_GPIOA:MCP_REG_GPIOB;
    uint8_t cur = 0;
    esp_err_t r = mcp23017_v2_reg_read8((mcp23017_v2_handle_t)handle, dev_idx, reg, &cur);
    if (r != ESP_OK) return r;
    uint8_t next = (cur & (uint8_t)(~mask)) | (value & mask);
    // write to OLAT to update outputs
    uint8_t reg_olat = (port==MCP_PORT_A)?MCP_REG_OLATA:MCP_REG_OLATB;
    return mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, reg_olat, next);
}

esp_err_t mcp23017_port_set_dir(void *handle, int dev_idx, mcp23017_port_t port, uint8_t dir_mask)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    uint8_t reg = (port == MCP_PORT_A) ? MCP_REG_IODIRA : MCP_REG_IODIRB;
    return mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, reg, dir_mask);
}

esp_err_t mcp23017_port_set_pullup(void *handle, int dev_idx, mcp23017_port_t port, uint8_t mask, mcp23017_pull_t pull)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    uint8_t reg = (port == MCP_PORT_A) ? MCP_REG_GPPUA : MCP_REG_GPPUB;
    if (pull == MCP_PULLUP_ENABLE) {
        return mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, reg, mask);
    } else {
        uint8_t cur = 0;
        esp_err_t r = mcp23017_v2_reg_read8((mcp23017_v2_handle_t)handle, dev_idx, reg, &cur);
        if (r != ESP_OK) return r;
        cur &= (uint8_t)(~mask);
        return mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, reg, cur);
    }
}

esp_err_t mcp23017_v2_read_gpio16(void *handle, int dev_idx, uint16_t *value)
{
    if (!handle || !value) return ESP_ERR_INVALID_ARG;
    uint8_t a,b;
    esp_err_t r1 = mcp23017_v2_reg_read8((mcp23017_v2_handle_t)handle, dev_idx, MCP_REG_GPIOA, &a);
    if (r1 != ESP_OK) return r1;
    esp_err_t r2 = mcp23017_v2_reg_read8((mcp23017_v2_handle_t)handle, dev_idx, MCP_REG_GPIOB, &b);
    if (r2 != ESP_OK) return r2;
    *value = (((uint16_t)a) << 8) | b;
    return ESP_OK;
}

esp_err_t mcp23017_v2_write_gpio16(void *handle, int dev_idx, uint16_t value)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    uint8_t a = (uint8_t)((value >> 8) & 0xFF);
    uint8_t b = (uint8_t)(value & 0xFF);
    esp_err_t r1 = mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, MCP_REG_OLATA, a);
    if (r1 != ESP_OK) return r1;
    return mcp23017_v2_reg_write8((mcp23017_v2_handle_t)handle, dev_idx, MCP_REG_OLATB, b);
}

esp_err_t mcp23017_v2_auto_setup(mcp23017_v2_bundle_t *out_bundle, bool apply_defaults)
{
    if (!out_bundle) return ESP_ERR_INVALID_ARG;
    memset(out_bundle, 0, sizeof(*out_bundle));

    // Try to obtain an existing I2C master bus handle (prefer I2C_NUM_0 then I2C_NUM_1)
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_master_get_bus_handle(I2C_NUM_0, &bus) != ESP_OK) {
        if (i2c_master_get_bus_handle(I2C_NUM_1, &bus) != ESP_OK) {
            ESP_LOGW(TAG, "No existing I2C master bus found (tried 0 and 1)");
            return ESP_ERR_NOT_FOUND;
        }
    }

    uint8_t addrs[8];
    i2c_master_dev_handle_t devs[8];
    int found = 0;
    esp_err_t r = v2_discover_devices_on_bus(bus, addrs, devs, &found);
    if (r != ESP_OK) return r;
    if (found == 0) {
        ESP_LOGI(TAG, "No MCP23017 devices discovered on bus");
        return ESP_ERR_NOT_FOUND;
    }

    mcp23017_v2_handle_t h = v2_handle_create_from_devices(bus, addrs, devs, found, false);
    if (!h) {
        // cleanup attached devs
        for (int i = 0; i < found; ++i) if (devs[i]) i2c_master_bus_rm_device(devs[i]);
        return ESP_ERR_NO_MEM;
    }

    out_bundle->handle = h;
    out_bundle->bus = (void*)bus;
    out_bundle->addr_count = found;
    out_bundle->owns_bus = false;
    for (int i = 0; i < found; ++i) out_bundle->addresses[i] = addrs[i];
    out_bundle->defaults_applied = false;

    if (apply_defaults) {
        bool all_ok = true;
        for (int i = 0; i < found; ++i) {
            esp_err_t rc = v2_apply_defaults_to_device(devs[i]);
            if (rc != ESP_OK) {
                ESP_LOGW(TAG, "Failed to apply defaults to device 0x%02X: %d", addrs[i], rc);
                all_ok = false;
            }
        }
        out_bundle->defaults_applied = all_ok;
    }

    ESP_LOGI(TAG, "v2: discovered %d MCP23017 device(s)", found);
    return ESP_OK;
}
