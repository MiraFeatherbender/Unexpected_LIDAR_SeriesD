# Dispatcher Pointer-Pool Design & Implementation Plan 📦

## Summary
A PSRAM-backed pool of refcounted dispatcher message objects (pointer messages) that are broadcasted by pointer to module queues (pointer-queues). Two allocation policies are supported: non-blocking `pool_try_alloc()` for streaming data (LIDAR, line sensor) and blocking `pool_alloc_blocking(timeout)` for control/override messages. This plan covers design, APIs, migration steps, tests, and metrics.

---

## Motivation & Goals
- Reduce internal-heap pressure caused by large value-queues storing full `dispatcher_msg_t` structures (1 KiB+ each).
- Avoid frequent malloc/free overheads by preallocating pool entries in PSRAM.
- Provide two delivery modes: best-effort non-blocking for high-volume streaming, and blocking/backpressure for control messages.
- Keep backward compatibility: modules opt-in to pointer-queue model; existing value-queue behavior remains supported.
- Compatibility goal: add a helper in `dispatcher_module.h` so modules can keep `process_msg(const dispatcher_msg_t *)` while queues store pointer messages; helper will unwrap the pointer and enforce payload lifetime rules.

Requirements
- Must support existing `dispatcher_msg_t` semantics (source, targets[], data[], message_len, context) while enforcing read-only rules for shared payloads.
- Must be safe under concurrent dispatch and consumption by multiple modules.
- Must provide clear metrics for pool usage, drops, and queue failures.
- Must have predictable latency for streaming (non-blocking) messages.

Constraints
- PSRAM available (8 MiB) — use `heap_caps_malloc/heap_caps_calloc(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` for pool allocation.
- FreeRTOS queues store pointers (item size = sizeof(ptr)); queue storage still allocated by FreeRTOS from internal heap.
- Task stacks must remain in internal RAM; do not put stacks in PSRAM.

---

## Design Overview
### Pool entry type
```
typedef struct pool_msg_s {
  atomic_uint_fast16_t ref; // atomic refcount
  dispatcher_msg_t msg;     // existing message layout
  // optional metadata: flags, timestamp
} pool_msg_t;
```
- Pool of `pool_msg_t` entries allocated in PSRAM.
- Free-list managed with a mutex or simple lock-free stack (single producer/consumer case possible optimization).

### Allocation policies
- `pool_try_alloc()` → returns `pool_msg_t*` or `NULL` immediately (non-blocking; used by streaming). Caller decides fallback (drop or fallback malloc).
- `pool_alloc_blocking(timeout_ms)` → waits for free entry up to timeout (used by control/override messages where delivery matters).
- `pool_free(pool_msg_t *m)` returns entry to the pool (internal use after refcount reaches 0).

### Broadcasting logic (dispatcher_broadcast_ptr)
- Accepts `pool_msg_t *p` and list of target modules.
- Attempts non-blocking enqueue to each target's pointer queue.
- For each enqueue success, atomically increment p->ref.
- After attempting all targets, if p->ref == 0 → pool_free(p) immediately (no receivers). Otherwise, keep p until consumers unref.
- If partial enqueues succeed (k > 0), p->ref is k — each consumer will call `msg_unref()` when done.
- Optionally, return metrics: successes, failures.

### Consumer behavior
- Consumes `pool_msg_t *p` from its queue.
- Must treat `p->msg` as read-only or call `msg_ref()` if it needs to retain beyond this immediate processing.
- After processing, call `msg_unref(p)` — atomic decrement; when it reaches 0, entry returns to the pool.

### Immutability & Context
- Document that `msg.data[]` and `msg.context` are valid while `ref > 0`.
- If `context` points to dynamically allocated structures, ensure it's PSRAM or shared and remains valid until unref.

---

## Pool sizing heuristic (boot-time)
- Discover: T = number of dispatch targets (TARGET_MAX or explicit count)
- Use parameters:
  - F = fanout fraction (default 0.25)
  - C = concurrency factor (default 2)
  - entries = clamp( ceil(T * F * C), min_entries=8, max_entries=1024 )
  - payload_size = expected payload size (bytes)
- Preallocate `entries` pool entries sized for worst-case payload and small overhead.

Notes: choose `F` & `C` after observing real traffic; make them configurable via Kconfig or runtime config.

---

## APIs (proposed)
- pool_init(entries, payload_size)
- pool_try_alloc() -> pool_msg_t* (non-blocking)
- pool_alloc_blocking(timeout_ms) -> pool_msg_t* (blocking)
- msg_ref(pool_msg_t *p)
- msg_unref(pool_msg_t *p)
- dispatcher_broadcast_ptr(pool_msg_t *p, dispatch_target_t *targets)
- Metrics: pool_total, pool_in_use, pool_alloc_failures, pool_drop_count, enqueue_failures

---

## Migration & Implementation Plan (staged)
1. Implement `dispatcher_pool` module (core pool + allocation APIs) with unit tests (no repo changes yet).  (2–3 days)
   - Mutex-protected free-list + semaphore for blocking alloc.
   - `pool_try_alloc()` and `pool_alloc_blocking()`.
   - `msg_ref()` / `msg_unref()` using __atomic ops.
   - Basic metrics via `ESP_LOGI` and counters.
2. Instrument dispatcher and modules with lightweight heap/free diagnostics temporarily (to validate baseline). (half day)
3. PoC: convert `line_sensor` (streaming) to use `pool_try_alloc()` + `dispatcher_broadcast_ptr()` and pointer-queue consumers. Validate dropping behavior under stress. (1–2 days)
4. PoC: convert `io_rgb` or `io_lidar` to use pool and pointer-queues for heavy payloads. Add robustness: fallback to malloc or drop policy and log metrics. (2–4 days)
5. Add optional blocking alloc path and convert control messages (e.g., io_battery overrides) to use blocking allocs. (1–2 days)
6. Add monitoring (metrics exposed via REST or console), adjust pool sizing, finalize docs & tests. (1–2 days)

Each PoC stage includes tests for correctness, concurrency, and memory usage.

---

## Testing & Validation
- Unit tests for pool alloc/free and refcount correctness (race tests if possible).
- Integration: stress tests that broadcast bursts to many targets and measure drops, latency, and heap usage.
- Track metrics: pool_used, pool_drops, enqueue_failures, average queue length.
- Safety tests: validate behavior when pool exhausted, queue full, or module crashes (ensure unref still happens appropriately).

---

## Observability & Ops
- Add `ESP_LOGI/W` around pool init failures, allocations, drops.
- Expose counters via a REST endpoint or simple `/api/metrics` JSON for operational visibility.
- Optionally add a debug CLI command to print pool state & stats.

---

## Rollback & Compatibility
- Keep existing dispatcher value-queue path intact and make pointer-queue opt-in per module.
- If issues appear, revert modules to earlier behavior quickly by switching queue type back and removing pointer-broadcast usage.

---

## Acceptance Criteria
- Pool is created at boot using heuristic and allocates successfully in PSRAM.
- `line_sensor` PoC uses pool: under stress, occasional messages drop but no memory or queue creation failures occur system-wide.
- `io_rgb` PoC demonstrates reduced internal-heap pressure (monitor `heap_caps_get_free_size(MALLOC_CAP_8BIT)`).
- No regressions in module behavior; refcount tests pass under concurrency.

---

## Open questions (to decide)
- Should we expose a REST API to observe / tune pool parameters at runtime? (useful for ops)
- Do we prefer non-blocking for streaming by default and blocking only for control messages? (recommended)

---

## Notes & References
- Use `heap_caps_*` APIs for PSRAM allocations (ESP-IDF).
- Use `__atomic_add_fetch` / `__atomic_sub_fetch` or C11 atomics for refcounts.
- Keep message payloads immutable while refcount > 0.

---

## Next step recommendation
I can implement the `dispatcher_pool` module as a PoC with `pool_try_alloc()` and `dispatcher_broadcast_ptr()` and convert `line_sensor` to use it. Would you like me to proceed with that PoC now? 

---

*Document created by Copilot-style design assistant — ready to iterate.*
