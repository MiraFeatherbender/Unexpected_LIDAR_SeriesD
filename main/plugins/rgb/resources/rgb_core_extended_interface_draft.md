# RGB Core Extended Interface (Draft v2)

Purpose: introduce phase-aware and noise-aware plugin sampling while preserving legacy plugin APIs and behavior.

This draft defines exact structs and function signatures for incremental migration.

---

## Design goals
- Keep existing `hsv_anim_t` and `rgb_anim_t` working unchanged.
- Allow outputs (onboard + strip) to request a color at a specific phase.
- Allow palette/noise mapping path where core receives noise input and returns mapped RGB.
- Let core own routing/fallback so outputs remain thin.

---

## Proposed public types (new)

```c
// rgb_core_ex.h
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "rgb_anim.h"

// Input mode determines which union arm is valid.
typedef enum {
    RGB_CORE_IN_HSV_PHASE = 0,   // base HSV + explicit phase_u8
    RGB_CORE_IN_NOISE_U8  = 1,   // direct noise sample for palette/noise mapping
} rgb_core_input_mode_t;

// Explicit caller-provided sampling input.
typedef struct {
    rgb_core_input_mode_t mode;
    union {
        struct {
            hsv_color_t base_hsv;
            uint8_t phase_u8;  // explicit phase/frame index in [0,255]
        } hsv_phase;
        uint8_t noise_u8;      // direct noise sample 0..255
    } in;
} rgb_core_sample_in_t;
```

---

## Proposed extended plugin callbacks (optional)

```c
// Optional extension for HSV plugins.
typedef struct {
    // Reset referenced phase state (typically to 0).
    void (*begin_phase)(uint8_t *phase_u8);
    void (*set_color)(hsv_color_t hsv);
    void (*set_brightness)(uint8_t b);

    // New: phase-aware HSV sample path.
    // Input mode must be RGB_CORE_IN_HSV_PHASE.
    // Output is RGB for output uniformity.
    bool (*sample_hsv_rgb)(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);
} hsv_anim_ex_t;

// Optional extension for RGB/noise plugins.
typedef struct {
    // Reset referenced phase state (typically to 0) when plugin uses phase.
    void (*begin_phase)(uint8_t *phase_u8);
    void (*set_brightness)(uint8_t b);

    // New: noise-aware RGB sample path.
    // Input mode must be RGB_CORE_IN_NOISE_U8.
    // Output: mapped RGB color.
    bool (*sample_rgb)(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);
} rgb_anim_ex_t;
```

---

## Core registration APIs (additive)

```c
// Existing APIs remain:
// void io_rgb_register_hsv_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin);
// void io_rgb_register_rgb_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin);

// New optional ex registrations (per-plugin incremental migration):
void io_rgb_register_hsv_plugin_ex(rgb_plugin_id_t id, const hsv_anim_ex_t *plugin);
void io_rgb_register_rgb_plugin_ex(rgb_plugin_id_t id, const rgb_anim_ex_t *plugin);
```

---

## Core sample API (new)

```c
// Core decides active plugin route based on active plugin + input mode.
// Output is always RGB.
bool rgb_core_sample(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);
```

Canonical call examples:

```c
// Example A: HSV + explicit phase sampling
rgb_core_sample_in_t in_hsv = {
    .mode = RGB_CORE_IN_HSV_PHASE,
    .in.hsv_phase = {
        .base_hsv = (hsv_color_t){ .h = 10, .s = 255, .v = 180 },
        .phase_u8 = 64,
    },
};

rgb_color_t out_rgb = {0};
bool ok = rgb_core_sample(&in_hsv, &out_rgb);
```

```c
// Example B: direct noise sample -> palette/noise RGB mapping
rgb_core_sample_in_t in_noise = {
    .mode = RGB_CORE_IN_NOISE_U8,
    .in.noise_u8 = 173,
};

rgb_color_t out_rgb = {0};
bool ok = rgb_core_sample(&in_noise, &out_rgb);
```

Compatibility helper (retain existing behavior):

```c
// Existing onboard path can continue calling this:
bool rgb_core_step_rgb(rgb_color_t *out_rgb);
```

Implementation note: `rgb_core_step_rgb()` can internally call `rgb_core_sample()` with synthesized input derived from current command state.

---

## Routing + fallback rules (in core)
1. If active plugin has EX registration:
    - EX-HSV plugin: call `sample_hsv_rgb(...)` when mode is `RGB_CORE_IN_HSV_PHASE`.
    - EX-RGB plugin: call `sample_rgb(...)` when mode is `RGB_CORE_IN_NOISE_U8`.
2. Else fallback to legacy registration:
   - legacy HSV plugin path (`step(hsv*)`) -> convert HSV to RGB as needed.
   - legacy RGB plugin path (`step(rgb*)`).
3. If plugin callback fails/returns false, fallback to legacy path if present; otherwise return false.
4. If input mode doesn't match plugin capability, either fallback to legacy path or return false by policy.
5. On plugin-change/reset events, core may call `begin_phase(&phase_u8)` to preserve existing begin semantics while externalizing phase storage.

---

## Phase semantics (draft)
- `in.hsv_phase.phase_u8` is caller-owned explicit sampling phase/frame index.
- Domain is `0..255` to match existing animation framing (heartbeat LUT + breathe 256-frame model).
- Natural progression wraps modulo 256 (e.g., `phase_u8 = (phase_u8 + step) & 0xFF`).
- Core should not mutate caller-provided phase.
- Plugins that do not use phase simply ignore it.
- EX begin behavior should reset referenced phase (typically to `0`) instead of relying on plugin-static phase globals.

---

## Migration plan (plugin-by-plugin)
1. Add new EX types + registration + `rgb_core_sample()` with fallback.
2. Convert one strip-selected plugin first (register EX + legacy retained).
3. Make strip sampling path call `rgb_core_sample()` with `noise_u8`/`phase` input.
4. Convert HSV plugins incrementally to EX (`sample_hsv`).
5. Convert dynamic plugin to EX (`sample_rgb`) after walk/noise backend update.
6. Remove legacy callbacks only when all plugins are migrated and verified.

---

## Non-goals for this phase
- No immediate change to JSON schema.
- No forced rewrite of all plugins.
- No breaking changes to existing API consumers.
