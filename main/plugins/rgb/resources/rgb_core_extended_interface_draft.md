# RGB Core Extended Interface (Draft v3)

Purpose: introduce phase-aware and noise-aware plugin sampling while preserving legacy plugin APIs and behavior.

This draft defines exact structs and function signatures for incremental migration.

---

## Design goals
- Keep existing `hsv_anim_t` and `rgb_anim_t` working unchanged.
- Allow outputs (onboard + strip) to request a color at a specific phase.
- Allow palette/noise mapping path where core receives noise input and returns mapped RGB.
- Let core own routing/fallback so outputs remain thin.

---

## Implemented core sample types (current)

```c
// rgb_core_ex.h
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "rgb_anim.h"

// Explicit caller-provided sampling input.
typedef struct {
    uint8_t plugin_id;         // requested plugin for this output/sample
    uint8_t brightness;        // explicit brightness for this sample (0..255)
    union {
        struct {
            hsv_color_t base_hsv;
            uint8_t *phase_u8; // caller-owned phase pointer (plugin mutates progression)
        } hsv_phase;
        uint8_t noise_u8;      // direct noise sample 0..255
    } in;
} rgb_core_sample_in_t;
```

---

## Extended plugin callbacks (optional, implemented shape)

```c
// Optional extension for HSV plugins.
typedef struct {
    // Reset referenced phase state (typically to 0).
    void (*begin_phase)(uint8_t *phase_u8);
    void (*set_color)(hsv_color_t hsv);
    void (*set_brightness)(uint8_t b);

    // New: phase-aware HSV sample path.
    // Core performs HSV->RGB conversion centrally.
    bool (*sample_hsv)(const rgb_core_sample_in_t *in, hsv_color_t *out_hsv);
} hsv_anim_ex_t;

// Optional extension for RGB/noise plugins.
typedef struct {
    // Reset referenced phase state (typically to 0) when plugin uses phase.
    void (*begin_phase)(uint8_t *phase_u8);
    void (*set_brightness)(uint8_t b);

    // New: noise-aware RGB sample path.
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

## Core sample API

```c
// Core decides route by requested plugin_id and registered plugin type.
// Output is always RGB.
bool rgb_core_sample(const rgb_core_sample_in_t *in, rgb_color_t *out_rgb);
```

Canonical call examples:

```c
// Example A: HSV plugin sampling with caller-owned phase
rgb_core_sample_in_t in_hsv = {
    .plugin_id = RGB_PLUGIN_BREATHE,
    .brightness = 200,
    .in.hsv_phase = {
        .base_hsv = (hsv_color_t){ .h = 10, .s = 255, .v = 180 },
        .phase_u8 = &phase_u8,
    },
};

rgb_color_t out_rgb = {0};
bool ok = rgb_core_sample(&in_hsv, &out_rgb);
```

```c
// Example B: direct noise sample -> palette/noise RGB mapping
rgb_core_sample_in_t in_noise = {
    .plugin_id = RGB_PLUGIN_FIRE,
    .brightness = 180,
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
1. Core uses registry type for `in->plugin_id` to choose path:
    - HSV-registered plugin -> EX `sample_hsv(...)` (if present), then core converts HSV->RGB.
    - RGB-registered plugin -> EX `sample_rgb(...)` (if present).
2. In EX path, `in->brightness` is an input parameter; plugin-defined behavior determines how/if it affects output.
3. Else fallback to legacy registration:
   - legacy HSV plugin path (`step(hsv*)`) -> convert HSV to RGB as needed.
   - legacy RGB plugin path (`step(rgb*)`).
4. If plugin callback fails/returns false, fallback to legacy path if present; otherwise return false.
5. On plugin-change/reset events, core may call `begin_phase(&phase_u8)` to preserve existing begin semantics while externalizing phase storage.

---

## Phase semantics (current)
- `in.hsv_phase.phase_u8` is caller-owned phase pointer.
- Domain is `0..255` to match existing animation framing (heartbeat LUT + breathe 256-frame model).
- Natural progression wraps modulo 256 (e.g., `phase_u8 = (phase_u8 + step) & 0xFF`).
- Plugins mutate caller-owned phase progression; callers own storage/lifetime.
- Plugins that do not use phase simply ignore it.
- EX begin behavior should reset referenced phase (typically to `0`) instead of relying on plugin-static phase globals.

---

## Migration status (as of 2026-02-24)
- [x] Begin signatures migrated to phase-pointer semantics and onboard owns/passes phase storage.
- [x] Core EX scaffolding added (`*_ex` registrations + `rgb_core_sample()`), with strict fallback.
- [x] Onboard kept on compatibility path (`rgb_core_step_rgb()`) for current behavior stability.
- [x] HSV plugins extended to EX while retaining legacy registrations (`OFF`, `SOLID`, `HEARTBEAT`, `BREATHE`).
- [x] Strip migrated to `rgb_core_sample()` and validated on hardware.
- [x] Caller-side mode selection removed; core now auto-routes by registered plugin type for requested plugin id.
- [x] Dynamic EX scaffold added non-breakingly; palette mapping helpers now support plugin-aware lookup and future float-noise rescale.

## Remaining migration items
1. Move dynamic from compatibility EX scaffold to full per-output instance-ready behavior (independent onboard/strip state ownership).
2. Finalize walk/noise contract for RGB plugins:
    - near-term: dynamic as palette retriever/mapper and random-walk return route;
    - later: normalize walk/noise interfaces per-output without breaking parity.
3. Transition onboard from legacy step path to sample path once RGB dynamic parity is verified.
4. Remove legacy callback reliance only after parity across onboard + strip is confirmed for all active plugins.

### Dynamic instancing plan (next)
Goal: allow onboard and strip to request dynamic RGB plugins independently without shared mutable runtime state.

1. Introduce dynamic instance state structs (caller-owned handle or core-owned instance slots):
    - selected plugin id
    - walk state / phase references (per output)
    - brightness input state (if needed by instance)
2. Keep palette assets shared and immutable:
    - cache palettes per plugin once (global read-only cache)
    - instance state references cached palette by plugin id
3. Keep compatibility path intact:
    - legacy `dynamic_step` remains bound to default instance used by onboard today
    - new sample callers (strip first) use explicit instance-aware path
4. Add instance-aware sample API surface (additive):
    - sample by `(instance, plugin_id, noise)` with plugin-type auto-routing still in core
    - no behavior change for existing onboard caller until explicitly migrated
5. Migrate outputs incrementally:
    - strip uses its own dynamic instance first
    - onboard migrates later after parity verification
6. Only after parity:
    - remove reliance on shared mutable globals in dynamic plugin runtime
    - retire compatibility path if no longer needed

Acceptance criteria:
- Changing strip dynamic plugin/walk does not alter onboard output.
- Changing onboard dynamic plugin/walk does not alter strip output.
- Palette lookups remain plugin-correct for both outputs with no startup WDT regressions.

---

## Non-goals for this phase
- No immediate change to JSON schema.
- No forced rewrite of all plugins.
- No breaking changes to existing API consumers.
