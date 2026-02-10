# Indexed Code Summary — dispatcher & display (quick lookup)

Purpose: fast reference mapping of important symbols, APIs, and suggested edit
locations for working on the dispatcher subsystem and display/LVGL stack.

Dispatcher core
- Files:
  - main/dispatcher.c — pointer-queue registry and broadcast helpers
  - main/dispatcher.h — public types (`dispatcher_msg_t`), `dispatcher_fill_targets()`
  - main/modules.def — module target/source list (used by enums)
- Key symbols:
  - `dispatcher_register_ptr_queue(target, queue)` — register queue for a target
  - `dispatcher_get_ptr_queue(target)` — return registered queue
  - `dispatcher_broadcast_ptr(pool_msg_t *msg, targets)` — send pointer messages to targets

Pointer pool (allocation & lifecycle)
- Files:
  - main/dispatcher/dispatcher_pool.h
  - main/dispatcher/dispatcher_pool.c
- Key patterns:
  - Allocate: `dispatcher_pool_try_alloc(type)` or `dispatcher_pool_alloc_blocking(type, timeout)`
  - Access message: `dispatcher_pool_get_msg()` / `_const()` (returns data pointer, message_len, source, targets)
  - Reference counting: `dispatcher_pool_msg_ref()` / `dispatcher_pool_msg_unref()` — always unref when finished
  - Helper send: `dispatcher_pool_send_ptr()` / `dispatcher_pool_send_ptr_params()` to allocate+send in one call
  - Common bug to watch: double-unref (pool logs warnings and increments `double_free_count`)

Module pattern
- Files:
  - main/dispatcher/dispatcher_module.h
  - main/dispatcher/dispatcher_module.c
- Usage:
  - Define a `dispatcher_module_t` with `process_msg()` and `step_frame()` hooks
  - Create/register pointer queue with `dispatcher_ptr_queue_create_register(target, queue_len)`
  - Start module with `dispatcher_module_start(module)` which spawns the standardized pointer-task
  - The task unwraps `pool_msg_t *` into `dispatcher_msg_t` via `dispatcher_module_process_ptr_compat()`

LVGL / display port (esp_lvgl_port)
- Files (major entrypoints):
  - managed_components/espressif__esp_lvgl_port/include/esp_lvgl_port.h
  - managed_components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port.c
  - managed_components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c
- Key APIs:
  - `lvgl_port_init(&cfg)` / `lvgl_port_deinit()` — initialize LVGL task/timer
  - `lvgl_port_lock(timeout_ms)` / `lvgl_port_unlock()` — serialize access to LVGL API
  - `lvgl_port_add_disp(&disp_cfg)` — register an LVGL display (returns `lv_display_t *`)
  - `lvgl_port_flush_ready(disp)` — notify flush-complete when using manual transfers
  - `lvgl_port_task_wake(event, param)` — notify LVGL task of external events

OLED example (working base)
- Files:
  - main/plugins/io_i2c_oled.c — initialization, `lvgl_port_add_disp()` and example "Hello World" label
  - main/plugins/io_i2c_oled.h
- Edit pointers:
  - To add UI pages: create `main/ui/ui_<feature>.c` and use `lv_obj_*` APIs under `lvgl_port_lock()`/`unlock()`
  - To change initial demo text/size: edit `io_i2c_oled.c` near LVGL label creation

SH1107 driver
- Files:
  - managed_components/espressif__esp_lcd_sh1107/include/esp_lcd_sh1107.h
  - managed_components/espressif__esp_lcd_sh1107/esp_lcd_sh1107.c
- Notes: use `ESP_LCD_IO_I2C_SH1107_CONFIG()` for I2C IO config; `esp_lcd_new_panel_sh1107()` creates the panel handle used by `lvgl_port_add_disp()`.

Quick how-to: send a motor PWM control message via dispatcher
1. Allocate message: `pool_msg_t *m = dispatcher_pool_send_ptr(DISPATCHER_POOL_CONTROL, SOURCE_XXX, targets, data, data_len, ctx)` or allocate then fill: `dispatcher_pool_try_alloc()` + set fields
2. Use `dispatcher_broadcast_ptr(m, targets)` (or let `dispatcher_pool_send_ptr` send for you)
3. Consumers created via `dispatcher_ptr_queue_create_register(TARGET_MOTOR, len)` will receive `pool_msg_t *` in their pointer-task

Suggested next files to scan (if you want deeper indexing):
- managed_components/* touch/knob/button source files (input devices)
- main/plugins/io_* modules that produce or consume dispatcher messages (e.g., motor, encoder)

---
Generated: 2026-02-10 — assistant
