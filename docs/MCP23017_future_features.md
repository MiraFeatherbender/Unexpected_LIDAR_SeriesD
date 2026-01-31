# MCP23017 Future Features Roadmap

This document captures additional features and improvements deferred for later evaluation and potential implementation in future versions.
Each item includes a short description, motivation, acceptance criteria, rough effort, and risk notes to help prioritize later.

---

## 1. Robust Batching & Bank-Mode (v3)
- See `MCP23017_v3_batching_and_bank_mode.md` for full proposal.
- Motivation: reduce I2C transactions, handle BANK/SEQOP layouts, and provide atomic multi-register RMW.
- Acceptance: contiguous block RMW works for SEQOP devices; banked devices handled with per-bank blocks; opt-in safe SEQOP toggle documented.
- Effort: medium (prototype + tests).
- Risk: IOCON toggling is risky; keep opt-in.

---

## 2. Tests & Example Harness
- Description: add `examples/mcp23017` demo and unit tests using a real-device hardware test harness; optionally create lightweight host-run unit tests with an `i2c_master` mock.
- Motivation: prevent regressions and verify real-world behavior across boards.
- Acceptance: test suite runs in CI/hardware lab and validates discovery, `config_port()`, ISR notification, and error paths.
- Effort: low→medium.
- Risk: hardware-in-loop test maintenance.

---

## 3. Documentation & Usage Guide
- Description: comprehensive docs for the `mcp23017` API, ISR registration patterns, examples (interrupt-driven and polling), and known hardware limits (encoder note).
- Motivation: easier adoption and fewer integration mistakes.
- Acceptance: README + `docs/` pages with sample wiring, code snippets, and caveats.
- Effort: low.

---

## 4. Thread-safety / ISR-safety Audit
- Description: mark which APIs are ISR-safe, add bus mutexing for multi-register transactions, and document concurrency model.
- Motivation: avoid accidental I2C from ISR and race conditions between batched ops and per-register helpers.
- Acceptance: clear docs + tested mutex around batched sequences.
- Effort: low→medium.

---

## 5. Recovery Hooks & Soft Reinit
- Description: explicit `mcp23017_reinit()` and `mcp23017_reset_to_defaults()` helpers; optional callback hooks for plugins to attach recovery logic.
- Motivation: robust runtime recovery from persistent I2C/device errors (watchdog-triggered recovery).
- Acceptance: APIs exist and plugin example demonstrates a recovery path restoring defaults and re-registering ISRs.
- Effort: low.

---

## 6. Per-Device Persisted Configuration (NVS)
- Description: optional NVS-backed per-device preferences and default table storage to persist device-specific configs across power cycles.
- Motivation: simplify per-device customization without source changes.
- Acceptance: API to save/restore defaults per-device and docs on NVS usage.
- Effort: medium.

---

## 7. I2C Mock Layer (optional)
- Description: lightweight mock `i2c_master` implementation enabling host-run unit tests and CI without hardware.
- Motivation: faster iteration, CI-friendly tests.
- Acceptance: unit tests pass with mock.
- Effort: medium.

---

## 8. 16-bit Atomic Ops & Masked Helpers
- Description: add and test `mcp23017_read_gpio16()` / `mcp23017_write_gpio16()` and masked 16-bit RMW helpers.
- Motivation: convenient atomic port operations and fewer error-prone two-register sequences in user code.
- Acceptance: helpers behave atomically (task context) and documented tradeoffs.
- Effort: low.

---

## 9. Runtime Health Check & Monitoring API
- Description: `mcp23017_v2_health_check()` that verifies I2C responsiveness and returns device state (last contact, errors, IOCON snapshot).
- Motivation: telemetry and watchdog integration for reliability monitoring.
- Acceptance: returns structured health status; used by example watchdog task.
- Effort: low.

---

## 10. Example Plugin Enhancements
- Description: expand `main/plugins/io_MCP23017.c` demo to show multi-device registration, per-device configs, recovery flow, and better comments for integrators.
- Motivation: serve as a comprehensive integration reference.
- Acceptance: plugin demonstrates typical usage patterns and recovery handling.
- Effort: low.

---

## Prioritization Guidance
- Near-term (low effort, high value): Documentation, tests, recovery hooks, 16-bit helpers, health-check.
- Mid-term: Per-device persisted config, enhanced example plugin, ISR audit and bus mutexing (if not already done).
- Long-term (higher effort/risks): v3 batching & bank-mode toggle, i2c mock.

---

If you'd like, I can convert any of the above into a tracked task with acceptance criteria and estimate in the TODO list for future planning.