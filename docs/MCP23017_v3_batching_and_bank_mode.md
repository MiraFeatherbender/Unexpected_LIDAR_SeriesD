# MCP23017 v3 — Batching & Bank-Mode Improvements

Goal
- Provide a robust, high-performance batched register RMW path that correctly handles both IOCON/BANK layouts, minimizes I2C transactions, and preserves device state on errors.

Motivation
- Current v2 implements a best-effort sequential batch write but falls back to per-register RMW. Some devices/configurations use BANKed register addressing, reducing the effectiveness of sequential batching.
- v3 should offer deterministic behavior, better performance, atomicity guarantees for multi-register updates, and safer opt-in mechanisms for toggling IOCON.SEQOP when needed.

Design summary
1. Addressing mode detection
   - On device attach (auto_setup) read and cache IOCON for each device.
   - Expose a cached `device_iocon` in the v2 handle (v3 internal) and a helper `mcp23017_v3_get_iocon(device_idx)` for diagnostics.
   - Use the cached IOCON.BANK and IOCON.SEQOP bits to select the write/read path.

2. Sequential batched RMW (SEQOP-friendly path)
   - Compute the minimal contiguous register window covering the registers to change (skip read-only registers when possible).
   - Block read the window once, modify only target bytes, then write the whole window back in one multi-byte transmit.
   - Use `OLATx` for output writes where appropriate to avoid GPIO read/write races.
   - Guard the entire operation with a bus mutex (serialize with other I2C ops).

3. BANK-mode handling
   - If IOCON.BANK=1 (banked mode): construct two smaller sequential windows (bank A and bank B) for the affected registers and do block RMW per-bank.
   - As an advanced/opt-in feature, provide a safe `force_seqop_batch` flag that temporarily clears BANK or sets SEQOP=0, performs batched work, then restores IOCON (document risks: some devices may glitch on IOCON changes).

4. Atomicity, retries, and rollback
   - Acquire bus lock for entire multi-register transaction.
   - On write failure: retry a limited number of times with short backoff.
   - After persistent failure, read-back the affected registers and attempt to restore prior state where possible; otherwise return a clear error and leave device in a consistent (readable) state.

5. Optimizations & byte savings
   - Only include changed bytes in write payloads; coalesce adjacent modified bytes into smallest number of contiguous writes.
   - Cache IOCON and other stable registers to minimize redundant reads; invalidate cache after write failures.

6. API and flags (v3 additions)
   - Extend `mcp23017_v2_config_port()`/new `mcp23017_v3_config_port()` with optional flags:
     - `MCP_CFG_FORCE_SEQOP` — temporarily enable sequential batching via IOCON change (opt-in)
     - `MCP_CFG_NO_BATCH` — disable batching, force per-register RMW
     - `MCP_CFG_BATCH_RETRIES` — internal retry policy (optional)
   - Keep defaults safe: automatic detection and use of best path; explicit opt-in required for IOCON toggling.

7. Concurrency & ISR safety
   - Document that batched operations are not ISR-safe; they require task context and must hold the bus mutex.
   - Ensure small 8-bit helpers remain available for ISR-safe use (if possible).

8. Testing & verification
   - Unit tests (mocked i2c_master): validate read-modify-write correctness for both BANK=0 and BANK=1.
   - Hardware tests: exercise both sequential and banked devices (if available), measure I2C transaction counts.
   - Stress tests: interleave ISR-triggered per-register ops with batched ops to verify bus mutex prevents corruption.

9. Migration notes
   - v3 should remain backward-compatible at API surface where possible; new flags and helpers are opt-in.
   - Document in `docs/` how to opt into `force_seqop` and the risks.

Implementation plan (phased)
1. Add IOCON cache to device struct and `read_device_iocon()` helper.
2. Implement bus mutex around multi-register transactions (use FreeRTOS semaphore).
3. Implement contiguous-window computation and sequential block RMW path for SEQOP devices.
4. Implement banked-mode path for BANK=1.
5. Add flags for `force_seqop_batch` and safe IOCON toggle (with restore and logging).
6. Add retries, rollback logic, and tests.
7. Update examples and docs, then iterate based on test results.

Risks & caveats
- Toggling IOCON (BANK/SEQOP) may affect device behavior; this must be opt-in and documented.
- Multi-device atomicity across different devices is not provided; mutex only serializes per-bus transactions.
- Increased complexity must be balanced against real-world gains; keep per-register path as a safe fallback.

Estimated effort
- Prototype: 1–2 days
- Tests + docs: 1–2 days
- Stabilization & edge-case hardening: 1–3 days



*End of document.*
