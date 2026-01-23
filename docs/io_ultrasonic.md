# Ultrasonic Sensor (RCWL-1601 / HC-SR04 style) — Design & Decisions

🎯 **Goal**
- Add a lightweight `io_ultrasonic` plugin to measure distance using a trigger (TX) pulse and an echo (RX) pulse via the ESP32 RMT peripheral.
- Publish measurements through the existing dispatcher pointer pool (pointer messages) for low-latency consumers and also emit short log messages.

---

## Key Design Decisions ✅
- Use the **RMT peripheral** for both TX (trigger pulse) and RX (echo capture). RMT supports hardware-timed pulses and robust receive semantics.
- **Default RMT tick resolution:** ~58.333 µs per tick (resolution_hz = 17143). This maps **1 RMT tick ≈ 1 cm**, so distance in cm is an integer tick count. This simplifies math and keeps conversions trivial while providing acceptable (1 cm) resolution.
  - Rationale: simpler integer arithmetic (distance_cm = ticks), easy to reason about, and uses small memory/time budgets.
- Provide a Kconfig option to switch to a fine-grained **1 µs tick** (resolution_hz = 1_000_000) for sub‑cm precision if desired.

---

## Hardware / GPIO defaults
- Default TRIG pin: configurable via Kconfig (select board default in project Kconfig file).
- Default ECHO pin: configurable via Kconfig.
- Sensor compatibility: typical HC‑SR04 family and RCWL‑1601 that expose TRIG (output to sensor) and ECHO (input from sensor) are supported.

---

## RMT configuration (recommended defaults)
- TX channel config:
  - .gpio_num = <TRIG_GPIO>
  - .resolution_hz = 17143 (58.333 µs/tick)
  - .mem_block_symbols = minimal (small)
- RX channel config:
  - .gpio_num = <ECHO_GPIO>
  - .resolution_hz = 17143
  - .mem_block_symbols = 64 (recommendation)
  - rmt_receive_config_t::signal_range_min_ns = 1000 (1 µs) or suitable glitch reject
  - rmt_receive_config_t::signal_range_max_ns = e.g. 40 * 1e6 ns (40 ms) for up to 400 cm

---

## Measurement algorithm (high level)
1. TX: generate a short trigger pulse (e.g., 10 µs HIGH). Use RMT TX channel to fire a deterministic pulse.
2. Start RX receive (rmt_receive) and wait for the RMT reception to complete.
3. Parse received RMT symbols, summing **HIGH** duration segments that correspond to the echo pulse (the echo pulse width is the round‑trip time).
4. Convert summed ticks to distance using chosen tick mapping:
   - With 58.333 µs tick: distance_cm = echo_ticks
   - distance_mm = distance_cm * 10
5. Apply optional smoothing/filtering (median N=3 default) before publishing.

> Note: The echo is a single HIGH pulse whose total duration may be split across multiple RMT symbols; ensure you sum contiguous high-level durations.

---

## Message & Integration
- Publish pointer pool messages using `dispatcher_pool_send_ptr_params`:
  - **Target:** `TARGET_ULTRASONIC` (new target); also send short text to `TARGET_LOG`.
  - **Payload:** binary `uint16_t` representing distance in millimeters (or `uint16_t` cm if preferred), and `context` optionally for metadata.
  - Also add an SSE/log textual representation when `TARGET_LOG` is present.

Example: sending uint16_t mm via pointer pool
```c
uint16_t mm = (uint16_t)(distance_cm * 10);
dispatch_target_t targets[TARGET_MAX];
dispatcher_fill_targets(targets);
targets[0] = TARGET_ULTRASONIC;

dispatcher_pool_send_params_t params = {
  .type = DISPATCHER_POOL_STREAMING,
  .source = SOURCE_ULTRASONIC,
  .targets = targets,
  .data = (const uint8_t *)&mm,
  .data_len = sizeof(mm),
  .context = NULL
};
dispatcher_pool_send_ptr_params(&params);
```

---

## Defaults & Behavior
- **Sampling mode:** Periodic by default (100 ms), optional single-shot on demand via API.
- **Filtering:** Median filter (N=3) default; configurable/off.
- **Timeout/range:** Default `signal_range_max_ns` set for ~400 cm (40 ms). This is configurable via Kconfig.

---

## Tests & Validation
- Add a pointer queue test similar to `dispatcher_pool_test` that verifies send/receive path and expected numeric conversions.
- Validate on hardware with a known distance and compare to expected ticks.

---

## Future enhancements
- Multi‑sensor support (multiple channels/targets).
- Temperature compensation via a temperature sensor Kconfig or runtime value (speed of sound correction).
- Optional higher-resolution mode (1 µs) beyond Kconfig.

---

If you'd like, I can: ✅
- Add `docs` file (done).  
- Generate `main/plugins/io_ultrasonic.c`, `Kconfig.projbuild` entries, and a small pointer‑queue unit test using the defaults above — or adjust any defaults first.

Which next step should I take? 🔧