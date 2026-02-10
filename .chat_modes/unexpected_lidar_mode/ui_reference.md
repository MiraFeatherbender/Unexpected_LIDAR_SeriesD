# UI Reference — LVGL scope & goals (derived from cheat sheet)

Purpose: a compact reference that maps the Copilot cheat-sheet into prioritized
implementation goals and the exact LVGL features we need to enable. This lives
with the chat-mode files so the agent and you share a common, lightweight
roadmap.

Related chat-mode files:
- system_prompt.txt — persona & constraints
- user_prompt_template.md — how we adapt your requests
- ingest_list.json — files the agent indexed first
- indexed_summary.md — quick code references for dispatcher/LVGL
- changelog.md — append-only audit of edits

Base assumptions
- Display: 128×64 monochrome (SSD1306/SH1107) in landscape
- Input: rotary encoder (diff + button state) fed via dispatcher
- Architecture: dispatcher -> LVGL input adapter -> page manager -> pages
- OLED example module: `main/plugins/io_i2c_oled.c` is the working baseline

High-level goals (priority order)
1. Input adapter: implement dispatcher→LVGL indev driver that exposes
   `{diff, state, nav_mode}` and supports dual-mode (widget vs page nav).
2. Title bar component: reusable LVGL container with title label, battery
   indicator, and 1-px progress bar.
3. Page manager: container-per-page pattern with show/hide, page index,
   and progress updates; encoder-held rotates pages.
4. Motor Control page: slider for PWM, labels for speed/direction — wire
   slider changes to dispatcher CONTROL messages.
5. Battery page: bars/labels updated from dispatcher telemetry.
6. PID tuning page: three sliders (P/I/D) with numeric labels and reset.
7. Sensors/status page: read-only labels for encoder rate + future IMU.
8. Polish: icons, simple animations, dim/sleep behavior (optional later).

LVGL features required (minimal subset)
- Core: lv_init(), lv_timer_handler(), lv_display, lv_indev
- Widgets: lv_label, lv_bar, lv_slider, lv_obj (containers)
- Event system: lv_event_cb for widgets (button press, value change)
- Display drivers: use `lvgl_port_add_disp()` (from `esp_lvgl_port`) for
  monochrome full-buffer mode
- Input adapter: implement `lv_indev_drv` (encoder type) reading dispatcher
  state and converting to LV events

LVGL configuration hints (from cheat-sheet)
- `LV_COLOR_DEPTH = 1`, `LV_COLOR_1_BIT = 1` (monochrome)
- `LV_TICK_CUSTOM = 1` and provide lv_tick_inc via esp_timer (esp_lvgl_port handles this)
- `LV_MEM_CUSTOM = 1` optional if using custom allocator
- Fonts: include a small font (8px) and possibly one larger for title

Concurrency & safety
- Always wrap LVGL updates in `lvgl_port_lock(timeout)` / `lvgl_port_unlock()`.
- Do not update LVGL from ISR context — use dispatcher tasks to post updates.
- Use `dispatcher_pool_send_ptr()` / `dispatcher_pool_alloc_blocking()` patterns
  when sending messages from workers.

Suggested incremental tasks (concrete next steps)
1. Create `main/ui/ui_input_adapter.c` — implement dispatcher consumer, maintain
   latest encoder state, and provide an LVGL indev driver that reads it.
2. Create `main/ui/ui_titlebar.c` — API `ui_titlebar_create(parent)` returning
   handles for updating title/battery/progress.
3. Create `main/ui/ui_pages.c` — page manager with `ui_pages_show(index)`.
4. Create `main/ui/ui_motor_pwm.c` — Motor page UI + dispatcher binding.

Files to edit/inspect first (from ingest_list + repo)
- `main/plugins/io_i2c_oled.c` — LVGL + display init (Hello World)
- `managed_components/espressif__esp_lvgl_port/*` — port APIs & examples
- `main/dispatcher/*` — pool, module, allocator (message lifecycles)

Acceptance & testing notes
- Default: changes are `ask-before-apply`; patches will include a one-line
  changelog entry. Build and device verification steps are left to you.
- Basic validation: after implementing Input adapter + Title bar + Page
  manager, verify pages switch with encoder-held rotation and widget
  adjustments with rotation when released.

Notes on LVGL scope reduction
- We only need a small set of widgets: labels, bars, sliders, containers,
  and event callbacks. Many LVGL optional modules (e.g., complex charts,
  object animations, text areas) can be left disabled to keep footprint small.

If you want, I can now scaffold the first two files (`ui_input_adapter.c`,
`ui_titlebar.c`) as conservative, build-friendly patches — confirm `ask-before-apply`
or `auto-apply` and I will draft the patches. 

Generated: 2026-02-10 — assistant
