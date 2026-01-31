# Copilot Instructions for New ESP-IDF UM Series D + UART LIDAR Project

## ESP-IDF UM Series D + UART LIDAR Project — AI Agent Guide

### ESP-IDF Version: v5.5.2

---

## Big Picture Architecture
- Modular ESP-IDF project targeting ESP32-family, with a core dispatcher system for decoupled inter-module communication.
- Main logic and subsystem plugins live in `main/plugins/`, organized by hardware IO (io_*.c/h) and subsystem folders (e.g., RPLIDAR, rgb, wifi).
- Each io_* module implements hardware IO (UART, GPIO, RGB, USB, Wi-Fi) and registers dispatcher handlers for message routing.
- Subsystem folders (e.g., RPLIDAR, rgb) contain higher-level logic, protocol parsing, and animation/state management, often bridging hardware IO and application logic.
- Board features (LED, GPIO, I2C, etc.) are abstracted in `src/` and configured via Kconfig (`Kconfig.projbuild`).
- LIDAR integration is incremental: start with UART echo, refactor to FreeRTOS tasks, then add LIDAR data parsing and protocol handling in RPLIDAR/.
- External dependencies managed via `idf_component.yml` and `managed_components/` (e.g., cJSON, TinyUSB, LED strip, mDNS).

## Developer Workflows
- **Configure features:** Use `idf.py menuconfig` to set board options (Kconfig).
- **Build:** Use ESP-IDF extension or `idf.py build` (root or `main/`).
- **Flash:** Use ESP-IDF extension or `idf.py flash` (auto-detects serial port).
- **Monitor:** Use ESP-IDF extension or `idf.py monitor` for UART output.
- **Clean:** Use `idf.py fullclean` to reset build artifacts.
- **Component management:** Update dependencies via `idf.py add-dependency` or edit `idf_component.yml`.
- **Debugging:** Use ESP-IDF monitor, check `sdkconfig` for config issues, and review FreeRTOS task logs for concurrency bugs.

## Project-Specific Conventions
- **Source structure:**
	- `main/` for app logic, `src/` for board abstraction, `examples/` for reference apps.
	- Use modular C files and keep feature logic isolated for easy extension.
- **Naming:**
	- Files: lower_case_with_underscores.
	- Functions: camelCase or ModuleName_FuncName (see dispatcher).
	- Macros: UPPER_CASE_WITH_UNDERSCORES.
- **FreeRTOS:**
	- Prefer task-based concurrency for UART/LIDAR data handling.
	- Use queues for inter-task communication (see dispatcher pattern).
- **Kconfig:**
	- Add new board features via `Kconfig.projbuild` and reference in CMakeLists.txt.
- **Component integration:**
	- Use `idf_component.yml` for external libraries (see cJSON, TinyUSB, LED strip).
	- Managed components live in `managed_components/`.

 # Copilot Instructions — Unexpected_LIDAR_SeriesD (concise)

 Purpose: Give an AI coding agent the minimal, immediately actionable knowledge
 to be productive in this ESP-IDF LIDAR project.

 Big picture
 - Modular ESP-IDF app for ESP32-S3. Core pattern: a central dispatcher and
   plugin-style `main/` modules under `main/plugins/` (io_*.c) for hardware IO.
 - Components providing drivers live in `components/` (public headers in
   `components/<name>/include/`, impl in `components/<name>/src/`).

 Key files to inspect
 - `main/dispatcher.c`, `main/dispatcher.h`: message routing and task creation.
 - `main/main.c`, `main/modules.def`, `main/plugins/`: plugin load/registration.
 - `components/*/src/*.c` and `components/*/include/*.h`: driver APIs.
 - `Kconfig.projbuild` and `main/CMakeLists.txt`: feature flags and build entrypoints.
 - `idf_component.yml` and `managed_components/`: external deps (cJSON, TinyUSB,
   led_strip, mdns).

modules.def (dispatcher targets/sources)
- File: `main/modules.def` lists modules via `X_MODULE(name)` macros (example: `X_MODULE(_RGB)`).
- The `dispatcher.h` header includes `modules.def` to generate `TARGET_*` and `SOURCE_*` enums and name arrays — update `modules.def` when adding a new dispatcher target.
- After editing `modules.def` rebuild the project so the generated enums line up with `dispatch_target_t`/`dispatch_source_t` used across modules.
- Use the generated enums in code as `TARGET_RGB` / `SOURCE_RGB` and `dispatcher_fill_targets()` to build target arrays.

 Build / flash / monitor (quick)
 - Recommended target: `esp32s3`.
 - From repo root:

 ```bash
 idf.py set-target esp32s3
 idf.py build
 idf.py -p <PORT> flash monitor
 ```

 Project-specific conventions & patterns
 - Naming: files lower_case_with_underscores; exported functions use
   `ModuleName_FunctionName` or camelCase; macros UPPER_CASE.
 - Plugin pattern: each `main/plugins/io_*.c` registers handlers with the
   dispatcher and provides start/stop functions (see `io_MCP23017.c`).
 - ISR/worker pattern: ISRs register via component APIs, notify worker tasks
   using `vTaskNotifyGiveFromISR()` and workers use `ulTaskNotifyTake()`.
 - I2C devices/drivers: components may own/tear down buses—respect `owns_bus`
   semantics and use provided `auto_setup()` helpers where present.

 Editing rules (practical, non-aspirational)
 - Add public headers to `components/<name>/include/` and implement in
   `components/<name>/src/`; update the component CMake if adding sources.
 - Keep `esp_err_t` return semantics and use `ESP_LOG*` macros for logs.
 - When changing Kconfig options, update `Kconfig.projbuild` and `main/
   CMakeLists.txt` if sources depend on new flags.

 Quick searches
 - Driver APIs: `rg "<component>_"` (e.g., `rg "mcp23017_"`).
 - Dispatcher usage: `rg "dispatcher_register|dispatcher_post"`.

 If something's unclear, ask for exact extracts (e.g., full `MCP23017.c`, the
 dispatcher public API, or `main/CMakeLists.txt`) and I will expand those
 sections. Reply with feedback on any missing or confusing points.

Component highlights — MCP23017
- Location: `components/mcp23017/src/MCP23017.c` and `components/mcp23017/include/MCP23017.h`.
- Auto-setup: `mcp23017_auto_setup(out_devices, apply_defaults, user_bus_cfg)`
	- If `user_bus_cfg` provided the component creates the bus and sets `owns_bus`.
	- Otherwise it auto-detects existing I2C master buses (I2C_NUM_0/1) and attaches devices at 0x20..0x27.
- Register cache: mirrors registers 0x00..0x15 to reduce transactions; use
	`mcp23017_sync_registers()` to refresh and `mcp23017_invalidate_register_cache()` to invalidate.
- Batched writes: `MCP_CFG_BATCH_WRITE` causes the component to attempt a
	sequential block write of 0x00..0x15 using the cache; on failure it falls
	back to per-register read-modify-write operations.
- Bus serialization: per-bus mutex registry (`bus_lock()`/`bus_unlock()`) serializes multi-register transactions across handles sharing the same bus.
- ISR registry: `mcp23017_isr_register(cfg, worker_task)` notifies worker tasks via `vTaskNotifyGiveFromISR()`; supports up to 40 MCU GPIOs and 8 workers per GPIO.
- Recommended usage: prefer the high-level helpers (`mcp23017_config_port`, `mcp23017_port_read/write`) for typical operations; use low-level `mcp23017_reg_read8`/`mcp23017_reg_write8` and `mcp23017_reg_read_block` when precise control is needed.

Wifi submodule (quick)
- Location: `main/plugins/wifi/` (`wifi_http_server.c`, `wifi_sse.c`, `io_wifi_ap.c`, `wifi_sse.h`, `wifi_http_server.h`).
- Network modes: `io_wifi_ap` initializes Wi‑Fi, prefers STA (using `CONFIG_WIFI_STA_*`) and falls back to AP mode after retries; it starts mDNS and the HTTP server on success.
- HTTP server: `wifi_http_server_start()` starts `esp_http_server` on port 80 (ctrl_port 32769), serves static files from `/data`, supports uploads via `/upload/*`, and registers REST endpoints via `rest_endpoints.def`.
- REST -> dispatcher: handlers call `dispatch_from_rest()` which allocates a `pool_msg_t` (control pool), sets `msg->source = SOURCE_REST`, and broadcasts pointer messages to module pointer queues. JSON GET handlers use a `rest_json_request_t` with a semaphore to wait synchronously for a module to fill a JSON buffer.
- SSE: `wifi_sse` implements server-sent events, registers an SSE endpoint and a `dispatcher_module_t` (`wifi_sse_mod`) to receive dispatcher messages and broadcast them to connected SSE clients. Targets map to event names (e.g., `console`, `line_sensor`).
- Server considerations: `wifi_http_server` preallocates file and JSON buffers (uses SPIRAM when available), sets `max_uri_handlers` to accommodate routes + SSE, and registers SSE before the catch-all static handler to avoid masking routes.
- Where to look: `main/plugins/wifi/wifi_http_server.c`, `main/plugins/wifi/wifi_sse.c`, `main/plugins/io_wifi_ap.c`.

Dispatcher system (deep dive)
- Purpose: central, lock-free-style routing for messages between plugins/modules.
- Message models:
	- Inline messages: `dispatcher_msg_t` (fixed `BUF_SIZE` 1024) — simple, but
		less efficient for high-rate or large payloads.
	- Pointer messages: `pool_msg_t` from `main/dispatcher/dispatcher_pool.*` —
		allocated from a pool and sent via registered pointer-queues for efficiency.
- Pool types: `DISPATCHER_POOL_STREAMING` (high-rate data) and
	`DISPATCHER_POOL_CONTROL` (lower-rate control messages). Pool sizes are
	configurable via `/data/dispatcher_pool_config.json` (loaded by
	`dispatcher_allocator_*`, defaults in `dispatcher_allocator.c`).
- Typical recommended pattern: ISR -> minimal ISR handler -> notify a worker
	task -> worker allocates a `pool_msg_t` (try_alloc or alloc_blocking) -> fill
	message -> call `dispatcher_pool_send_ptr()` or `dispatcher_pool_send_ptr_params()`.
- Queue registration & handlers:
	- Use `dispatcher_register_ptr_queue(target, queue)` or the helper
		`dispatcher_ptr_queue_create_register()` (seen in `dispatcher_module.h`).
	- Modules can register direct handlers via `dispatcher_register_handler()`
		but pointer-queue + worker task is the preferred pattern for ISRs.
- Module template: `dispatcher_module_t` (see `dispatcher_module.h`) is the
	standard struct used by `dispatcher_module_start()` — it creates a pointer
	queue (if missing), spawns the standardized `_ptr` task, unwraps `pool_msg_t`
	into `dispatcher_msg_t` for your `process_msg()` callback, and provides an
	optional periodic `step_frame()` hook (configured via `step_ms`).
- Important runtime notes:
	- Always `dispatcher_pool_msg_ref()`/`unref()` correctly; the pool detects
		and logs double-unref and maintains `in_use` stats.
	- The module task logs queue-depth warnings when >75% full and rate-limits
		those warnings to ~10s.
	- Use `dispatcher_pool_alloc_blocking()` if you must wait for a buffer;
		otherwise `try_alloc()` returns immediately on pool exhaustion.
- Useful searches: `rg "dispatcher_pool_send_ptr|dispatcher_pool_msg_unref|dispatcher_register_ptr_queue|dispatcher_module_start"`.

Dispatcher patterns (quick checklist)
- ISR rules: keep ISRs minimal — either `vTaskNotifyGiveFromISR()` a worker or `xQueueSendFromISR()` into an ISR-safe queue. Do not allocate or block in ISR context.
- Worker task: consume ISR queue or notification, allocate a `pool_msg_t` (try_alloc or alloc_blocking), fill `msg->source`, `msg->data`/`message_len` or `msg->context`, call `dispatcher_pool_send_ptr()` / `dispatcher_pool_send_ptr_params()` and `dispatcher_pool_msg_unref()` when done.
- Pool choice: use `DISPATCHER_POOL_STREAMING` for high-rate telemetry (LIDAR, sensors) and `DISPATCHER_POOL_CONTROL` for REST/control flows.
- Targets: call `dispatcher_fill_targets(targets)` then set `targets[0] = TARGET_*` (or add multiple targets) before sending.
- Pointer queues: for modules that receive messages frequently or large payloads, register a pointer queue with `dispatcher_ptr_queue_create_register()` or `dispatcher_register_ptr_queue()` and consume `pool_msg_t *` directly from the queue.
- Module template: use `dispatcher_module_t` + `dispatcher_module_start()` to create a standard pointer-task that unwraps `pool_msg_t` into `dispatcher_msg_t` and calls your `process_msg()`; `step_frame()` provides periodic work scheduling.
- Refcounts: when sharing `pool_msg_t` across async consumers call `dispatcher_pool_msg_ref()` and always call `dispatcher_pool_msg_unref()` when finished; the pool logs double-unref for diagnostics.
- Queue depth and timing: tasks log warnings when queue >75% full; choose `queue_len`, `stack_size`, and `task_prio` accordingly and prefer non-blocking allocations where appropriate.
- TX flows: implement a pointer-queue consumer for transmit paths (example: LIDAR TX) — consumers read `pool_msg_t *`, use `dispatcher_pool_get_msg_const()` and `dispatcher_pool_msg_unref()` after transmit.
- REST sync pattern: for JSON GETs use a `rest_json_request_t` with a semaphore — REST handler dispatches a pointer message with `context=rest_ctx` and waits on `rest_ctx.sem` for the module to fill the buffer and signal.



