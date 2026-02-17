- Defaulted runtime accel state in `ui_setting_item` by force-resetting `accum`, `residual`, and `last_ms` after copying config, and removed explicit zero assignments for those fields from `ui_page_encoder_test` config.
- Restored page container styling in encoder/template pages to the original `lv_obj_add_style(..., &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT)` and removed transparent property overrides.
- Reverted unified page-container theme experiment: removed `ui_style_page_container` from `ui_styles` and restored direct transparent container properties in encoder/template pages.
- Flipped `ui_style_page_container` background/border colors (bg white, border black) to compensate for inverted OLED panel polarity while keeping deterministic container style settings.
- Made `ui_style_page_container` deterministic by explicitly setting border color/width, radius, padding, and disabling shadow/outline to avoid unintended theme fallback on page containers.
- Added shared `ui_style_page_container` in `ui_styles` and switched page containers (`ui_page_encoder_test`, `ui_page_template`) to use this theme style instead of inline transparency property calls.
- Made page containers transparent in `ui_page_encoder_test` and `ui_page_template` to prevent LVGL default panel backgrounds from overriding centralized tile background styling.
- Centralized page background styling in `ui_core` (tileview + tiles use shared style) and removed per-page container background styles from `ui_page_encoder_test` and `ui_page_template`.
- Tuned bar-mode vertical layout to be more even in the container by using dedicated symmetric offsets (`UI_SETTING_BAR_NAME_Y=-12`, `UI_SETTING_BAR_Y=12`).
- Adapted bar rendering to draw current numeric value in/near the fill via `LV_EVENT_DRAW_MAIN_END` callback (`ui_setting_item_bar_draw_value_cb`), replacing the separate bar value label.
- Cleaned `ui_setting_item`: removed slider widget API/implementation/references and deleted graphical fields from `ui_setting_item_config_t`; bar/label geometry is now centralized via internal constants in `ui_setting_item.c`.
- Added `UI_SETTING_WIDGET_BAR` with `ui_setting_item_create_bar()` and switched `ui_page_encoder_test` from slider to bar to provide a more reliable monochrome value indicator.
- Restored encoder-test slider to pre-regression padding-test profile (`remove_style_all=false`, `slider_pad_ver=4`, `slider_knob_pad=6`) and reverted knob border width to 1 in `ui_setting_item`.
- Fixed slider visibility regression by changing monochrome colors to `MAIN=white`, `INDICATOR=black`, and black-bordered white knob in `ui_setting_item`.
- Updated slider API/behavior to mirror LVGL example semantics: added `slider_remove_style_all`, `slider_main_pad_ver`, and `slider_knob_pad_all`; encoder test now uses `main_pad_ver=-2` and `knob_pad_all=6` with theme styles removed.
- Refactored `ui_setting_item` slider geometry to LVGL padding semantics (`slider_pad_ver`, `slider_knob_pad` + legacy `slider_knob_size` mapping), and set encoder test slider to thinner indicator via padding.
- Updated `ui_page_template` container and template label to use `ui_style_light_mode` for page-level style consistency.
- Switched page/widget defaults to `ui_style_light_mode` after OLED driver inversion: encoder test container and `ui_setting_item` name/value labels now default light, with selected name highlight using dark inversion.
- Added file-local macro toggle `IO_I2C_OLED_INVERT_COLORS` in `main/plugins/io_i2c_oled.c` and applied `esp_lcd_panel_invert_color()` during panel init.
2026-02-10: Created chat-mode package with system prompt, few-shots, template, ingest list, and README. (assistant)
2026-02-10: Refactored `io_i2c_oled.c` to remove embedded Hello World UI; added `main/ui/ui_hello.c` and `main/ui/ui_hello.h`. (assistant)
2026-02-10: Updated `main/CMakeLists.txt` to compile `ui/ui_hello.c` and added `ui` to `INCLUDE_DIRS`; updated `main.c` to call `ui_hello_show()`. (assistant)
2026-02-10: Added `external_docs.md` with LVGL API docs link for quick reference. (assistant)
2026-02-15: Centralized `ui_page_template` logic in source-only module (removed `ui_page_template.h`) and removed all `ui_hello.h` include references from UI/main sources. (assistant)
2026-02-15: Removed unused placeholder header `main/ui/ui_hello.h` after eliminating all include dependencies. (assistant)
2026-02-16: Added send-only `ui_dispatch_bridge` module (`main/ui/ui_dispatch_bridge.[ch]`), wired bridge init/deinit in `ui_core`, and connected `ui_page_encoder_test` commit-on-deselect to publish log messages via dispatcher `TARGET_LOG`. (assistant)
2026-02-16: Refactored `ui_dispatch_bridge` primary send API to accept `dispatcher_pool_send_params_t` and retained `ui_dispatch_bridge_send_text()` as a convenience wrapper. (assistant)
2026-02-16: Added enum/switch-based commit policies (`LIVE`, `ON_DESELECT`, `DEFERRED`) to `ui_page_encoder_test` with policy-driven bridge send triggers on rotate/deselect/page-hide. (assistant)
2026-02-16: Flattened `ui_page_encoder_test` commit policy filtering into a combined key switch (`policy<<8 | reason`) to remove nested switches and keep commit routing explicit. (assistant)
2026-02-16: Added `UI_COMMIT_KEY`/`UI_COMMIT_CASE` macros in `ui_page_encoder_test` to improve combined commit switch readability without changing behavior. (assistant)
2026-02-17: Added reusable `ui_setting_item` module with union-based value model, observer-bound labels, accel handling, commit policies, and bridge send integration; refactored `ui_page_encoder_test` to use it. (assistant)
2026-02-17: Fixed cross-file style-state highlighting by converting `ui_styles` from header-static definitions to shared globals in `ui_styles.c` and adding it to component build sources. (assistant)
2026-02-17: Added slider-backed adapter path to `ui_setting_item` (`ui_setting_item_create_slider`) and updated `ui_page_encoder_test` to validate slider + value label using the same commit/dispatch pipeline. (assistant)
2026-02-17: Added configurable slider width (`slider_width`) and applied `ui_style_dark_mode` to slider MAIN/INDICATOR/KNOB parts; set smaller slider width in `ui_page_encoder_test`. (assistant)
2026-02-17: Added configurable slider knob size (`slider_knob_size`) and applied it to `LV_PART_KNOB`; updated `ui_page_encoder_test` to size the knob smaller and stop forcing reduced slider track width. (assistant)
2026-02-17: Fixed slider dark-style application by using `LV_STATE_ANY` on slider MAIN/INDICATOR/KNOB style selectors (and knob size selectors) to avoid state-based theme overrides. (assistant)
2026-02-17: Switched slider MAIN/INDICATOR/KNOB style application to `ui_style_light_mode` (all states) as a temporary visual fix while refining part-specific color styling. (assistant)
2026-02-17: Replaced slider style attachment with explicit local part styling (MAIN/INDICATOR/KNOB) using black colors and full opacity across `LV_STATE_ANY` to improve visibility on the monochrome-inverted display. (assistant)
2026-02-17: Switched explicit local slider part colors (MAIN/INDICATOR/KNOB bg and KNOB border) from black to white to avoid dark silhouette overlay and improve visibility. (assistant)
