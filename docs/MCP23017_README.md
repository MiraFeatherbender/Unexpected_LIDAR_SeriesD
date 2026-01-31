# MCP23017 — Component README

This document describes the MCP23017 component: a friendly ESP-IDF driver for the Microchip MCP23017 I/O expander (I2C).
It gives a high-level overview, usage examples, and a concise reference for advanced users.

## Device overview
- Device: Microchip MCP23017 — a 16-bit I/O expander accessed over I2C.
- Key features: two 8-bit ports (A/B), configurable direction, internal pull-ups, interrupt-on-change per-pin, configurable interrupt polarity/ODR, and sequential/banked register addressing (IOCON.BANK/SEQOP).
- Typical use: expand MCU GPIOs for inputs or outputs, use INT lines for change notification, and perform port-level reads/writes.
- Limitations: I2C transaction latency means very high-rate edge sources (fast encoders) may exceed practical interrupt/read rates.

## Key goals of `mcp23017`
- Friendly high-level helpers for port-level operations and common patterns.
- Full-auto discovery: locate an existing I2C bus, discover MCP23017 devices on addresses 0x20..0x27, optionally apply datasheet defaults.
- Lightweight ISR registration plumbing that lets plugins register a worker task to be notified when a MCU GPIO INT fires.
- Batched port configuration with a safe fallback to per-register RMW.

## Files
- Header: [main/src/MCP23017.h](main/src/MCP23017.h#L1) — API, enums, and function docs.
- Implementation: [main/src/MCP23017.c](main/src/MCP23017.c#L1) — internal helpers, discovery, and ISR registry.
- Example plugin: [main/plugins/io_MCP23017.c](main/plugins/io_MCP23017.c#L1) — demonstrates `auto_setup`, `config_port`, and ISR registration.
- v3 proposal (future): [docs/MCP23017_v3_batching_and_bank_mode.md](docs/MCP23017_v3_batching_and_bank_mode.md)

## High-level usage (single-task example)
- Discover devices and apply defaults (optional):

```c
mcp23017_attached_devices_t devices = {0};
if (mcp23017_auto_setup(&bundle, true) != ESP_OK || bundle.handle == NULL) {
    ESP_LOGE(TAG, "auto-setup failed");
    return;
}
// Use the first discovered device
mcp23017_handle_t dev = bundle.handle;

// Configure Port B and Port A (inputs with pullups + interrupt-on-change)
mcp23017_pin_cfg_t cfg = {
    .port = MCP_PORT_B,
    .mask = MCP_PORT_ALL,
    .pin_mode = MCP_PIN_INPUT,
    .pullup = MCP_PULLUP_ENABLE,
    .int_mode = MCP_INT_ANYEDGE,
    .int_polarity = MCP_INT_OPENDRAIN,
    .flags = MCP_CFG_BATCH_WRITE,
};
(void)mcp23017_config_port(dev, 0, &cfg);

cfg.port = MCP_PORT_A;
(void)mcp23017_config_port(dev, 0, &cfg);
```

## Low-level helpers (for advanced users)
- `mcp23017_reg_read8(h, dev_idx, reg, &val)` — read a single register from a discovered device.
- `mcp23017_reg_write8(h, dev_idx, reg, val)` — write a single register.
- `mcp23017_port_read()` / `mcp23017_port_write()` — 8-bit port helpers.
- `mcp23017_read_gpio16()` / `mcp23017_write_gpio16()` — 16-bit wrappers operating on A/B together.
- `mcp23017_isr_register(cfg, worker_task)` / `mcp23017_isr_unregister()` — configure MCU GPIO ISR plumbing and register worker task notifications.
- `mcp23017_isr_get_hits(gpio)` — diagnostic counter for ISR hits on a given MCU GPIO.

## Auto-discovery vs manual device assignment
- `mcp23017_auto_setup()` will:
  - try to obtain an existing I2C master bus (I2C_NUM_0 then I2C_NUM_1),
  - probe addresses 0x20..0x27 and attach devices,
  - optionally apply datasheet defaults.
- Manual assignment: if you maintain your own bus and device setup, you can attach devices using the workspace `i2c_master` APIs and then create a handle from devices (see `MCP23017.c` helpers).

## Notes & best practices
- Keep ISR code minimal and do all I2C in a task.
- Use MCU pins that support internal pull-ups when wiring to MCP INT (or provide external pull-ups), and avoid reserved pins (UART TX/RX) for IRQ input.
- For high-rate encoders or signals, use MCU-native peripherals (PCNT/PCNT, RMT, MCPWM) instead of the MCP23017 INT path — the expander and I2C latency limit edge rates.

## Where to look next
- Implementation details and comments are in the header and source files:
  - [main/src/MCP23017.h](main/src/MCP23017.h#L1)
  - [main/src/MCP23017.c](main/src/MCP23017.c#L1)
- Future batching/bank-mode improvements are tracked in: [docs/MCP23017_v3_batching_and_bank_mode.md](docs/MCP23017_v3_batching_and_bank_mode.md)

---

If you want, I can also copy this README content to the project root `README.md` or add a short example file under `examples/`. Let me know which you prefer.
