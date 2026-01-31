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



