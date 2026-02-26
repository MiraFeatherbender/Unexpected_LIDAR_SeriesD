# RGB/UI Next-Phase Migration Plan

## Summary
This draft defines the next migration phase after EX-path stabilization: implement manual x/y/z noise offsets first, expand UI setting collections by category, and defer JSON persistence until behavior and UX are stable.

---

## Goals
- Add manual coordinate stepping controls for dynamic RGB noise sampling:
  - `x_offset`, `y_offset`, `z_offset`
  - optional step/rate controls for controlled calibration
- Keep offsets intentionally unbounded and non-normalized.
- Keep strip as the primary test output while features are dialed in.
- Add additional setting categories as separate tile collections (not one giant collection).
- Defer persistence work until collection behavior and routing are stable.

## Non-Goals (for this phase)
- Final per-output JSON schema and save/load implementation.
- Full onboard parity for all categories before strip validation completes.
- Re-enabling full random walk windows before manual stepping is validated.

---

## Core Decisions

### 1) Numeric representation for coordinates/walk inputs
Use **float as canonical runtime representation**.

Reasoning:
- `fnlGetNoise*` is float-native.
- Offsets are intentionally unbounded and non-normalized.
- Using float avoids repeated conversion and preserves intended calibration semantics.

Implementation note:
- UI widgets may still use scaled controls for ergonomics, but values should be converted to/stored as float in runtime state.

### 2) Collection strategy
Use **multiple setting collections** by concern (not a single mega collection):
- FNL settings (already present)
- Manual coordinate stepping (`x/y/z`)
- Walk spec ranges (`min_dx`, `max_dx`, `min_dy`, `max_dy`) later
- HSV test settings (for HSV plugins)
- Brightness test settings (all plugins)

### 3) Adaptive visibility/routing strategy
Do this in two stages:
1. Build each collection first and validate behavior.
2. Add plugin-aware collection visibility after each collection is stable.

Visibility model:
- Use **registration metadata** on each collection (not hard-coded resolver tables).
- Each collection declares plugin-type applicability via a small bitmask (example: `RGB`, `HSV`, `ALL`).
- Resolver iterates registered collections and filters by active plugin type.
- Output selection affects **routing target** (strip/onboard), not collection visibility.

This keeps behavior consistent with existing repository patterns (dispatcher/plugin/page registration) while keeping resolver logic generic.

#### Proposed metadata shape (draft)
```c
typedef enum {
  UI_COLLECTION_PLUGIN_TYPE_RGB = 1 << 0,
  UI_COLLECTION_PLUGIN_TYPE_HSV = 1 << 1,
  UI_COLLECTION_PLUGIN_TYPE_ALL = UI_COLLECTION_PLUGIN_TYPE_RGB | UI_COLLECTION_PLUGIN_TYPE_HSV,
} ui_collection_plugin_type_mask_t;

typedef struct {
  const ui_setting_collection_t *collection;
  uint32_t plugin_type_mask;   // ui_collection_plugin_type_mask_t bitmask
  uint16_t order;     // stable ordering among visible collections
} ui_collection_registration_t;
```

Resolver inputs (draft):
- Active plugin id/type from RGB core
- Active output route target (strip/onboard)

Resolver output (draft):
- Ordered list of visible collections for the current plugin type context

---

## Proposed Runtime Shape
- Keep a central settings runtime state for active calibration values (float-backed).
- Route settings updates through dispatcher/context contract already used by strip FNL snapshot messages.
- For this phase, apply changes to strip path first.
- Add onboard acceptance after strip behavior is verified.

---

## Minimal API Surface (draft)

This section proposes the smallest set of new symbols/files to enable registration-based collection visibility cleanly.

### New/extended types

```c
typedef enum {
  UI_COLLECTION_PLUGIN_TYPE_RGB = 1 << 0,
  UI_COLLECTION_PLUGIN_TYPE_HSV = 1 << 1,
  UI_COLLECTION_PLUGIN_TYPE_ALL = UI_COLLECTION_PLUGIN_TYPE_RGB | UI_COLLECTION_PLUGIN_TYPE_HSV,
} ui_collection_plugin_type_mask_t;

typedef struct {
  const ui_setting_collection_t *collection;
  uint32_t plugin_type_mask;
  uint16_t order;
} ui_collection_registration_t;
```

### New resolver API (candidate)

```c
// ui/pages/ui_settings_resolver.h
void ui_settings_resolver_register(const ui_collection_registration_t *entry);
void ui_settings_resolver_reset(void);

size_t ui_settings_resolver_resolve(
  uint32_t active_plugin_type_mask,
    const ui_setting_collection_t **out_collections,
    size_t out_max);
```

### Registration style
- Each settings collection module owns and registers its own metadata.
- Resolver remains generic and data-driven (no hard-coded plugin->collection tables).

---

## Settings Transport Contract v2 (draft)

This addendum defines the agreed hybrid model:
- **Session control plane** over dispatcher context messages.
- **Working-state data plane** via subscriber/mailbox updates during active edit.

### Why v2
- Current per-change synchronous RPC (send + wait + ack) creates avoidable dispatcher/semaphore churn.
- Pure subscribe/unsubscribe reduces traffic, but still needs explicit ownership/lifecycle semantics.
- Hybrid model keeps low-overhead updates plus deterministic session boundaries.

### Control plane operations
- `GET_SNAPSHOT` — fetch current effective state for selected domain/output.
- `BEGIN_EDIT` — claim edit session for `(output_target, domain)` and bind active editor.
- `END_EDIT` — release session; end reason supports `CANCEL`/`COMMIT` (commit may be no-op initially).

Notes:
- Control-plane operations remain synchronous (context + semaphore) for deterministic handshakes.
- UI must call `END_EDIT` on output switch and page hide/deinit.

### Data plane behavior during active session
- UI updates working values through lightweight mailbox/subscriber path (no per-tick sync ack).
- Output-side render path reads latest working values each frame/tick.
- Updates are coalesced naturally: newest value wins.

### Ownership and routing
- One active editor per `(output_target, domain)`.
- `BEGIN_EDIT` fails with busy status if already owned by another editor token.
- Output selection controls routing target only; collection visibility remains plugin-type driven.

### Suggested status codes
- `OK`
- `ERR_ARG`
- `ERR_SIZE`
- `ERR_BUSY`
- `ERR_ROUTE`
- `ERR_DOMAIN`
- `ERR_VERSION_OLD` (if versioning is used)

### Suggested phased adoption
1. Apply v2 to FNL/strip first.
2. Reuse same envelope for manual offsets (`x/y/z`).
3. Extend to HSV/brightness test domains.
4. Add true commit/restore once persistence model is finalized.

---

## ui_core Impact (likely)

`ui_core` currently treats pages as a fixed compile-time list from `pages.def` and navigates page tiles directly.
To support clean dynamic collection visibility, we likely need one of these approaches:

### Option A (minimal disruption, recommended)
- Keep a single settings page entry in `pages.def`.
- Inside settings page module, add an internal sub-resolver that swaps active collection set at runtime.
- `ui_core` remains mostly unchanged.

### Option B (broader refactor)
- Make `ui_core` page list itself dynamic by plugin context.
- Higher complexity; impacts page navigation/indexing and indicator assumptions.

Recommendation: start with **Option A** to avoid unnecessary `ui_core` churn, then revisit if global page-level dynamic visibility becomes necessary.

---

## Coverage Tracker (living matrix)

Use this table as a progress tracker while staying contract-first.

| Domain | Contract payload | UI collection ready | Strip applies | Onboard applies | Notes |
|---|---|---|---|---|---|
| FNL | `fnl_state` | ✅ Existing | ✅ Existing | ⏳ Later | Current active settings page/contract path |
| Offsets 3D | `rgb_settings_offsets_3d_t` | ⏳ Pending | ⏳ Pending | ⏳ Later | Phase A primary scope |
| Walk range | `rgb_settings_walk_range_t` (`speed_*`, `jitter_*`) | ⏳ Pending | ⏳ Pending | ⏳ Later | Design-only for now; runtime later |
| HSV test | `rgb_settings_hsv_test_t` | ⏳ Pending | ✅ Sampling path exists | ⏳ Pending acceptance | Collection/resolver integration pending |
| Brightness test | `rgb_settings_brightness_test_t` | ⏳ Pending | ✅ Sampling path exists | ⏳ Pending acceptance | Collection/resolver integration pending |

Status legend:
- ✅ done
- ⏳ planned/in progress

Tracking rule:
- Promote a row to ✅ only after behavior test is validated on target output (strip first).

---

## Phased Implementation Plan

### Phase A — Manual x/y/z stepping (strip first)
1. Add runtime fields for `x_offset`, `y_offset`, `z_offset` and optional per-axis step/rate.
2. Add a new UI settings collection page for manual stepping.
3. Wire page callbacks to strip settings contract and apply offsets in strip sampling path.
4. Verify smooth long-session adjustment with no UI stalls.

Acceptance criteria:
- Manual stepping visibly moves sampled pattern as expected.
- No normalization/clamping of coordinate offsets.
- Stable under prolonged encoder interaction.

### Phase B — Additional collections (still strip-first)
1. Add HSV test collection.
2. Add brightness test collection shared across plugin types.
3. Keep walk range collection disabled or gated until manual stepping is validated.

Acceptance criteria:
- HSV and brightness test controls behave deterministically.
- Category isolation is preserved (no cross-collection state corruption).

### Phase C — Plugin-aware collection visibility
1. Introduce collection registration metadata with plugin-type bitmask + order.
2. Implement a generic resolver that filters registered collections by plugin type.
3. Keep output routing separate from visibility logic.
4. Preserve existing settings-page contract while swapping active collection set.

Acceptance criteria:
- Collection visibility updates correctly on plugin type change.
- UI page transitions remain responsive and non-blocking.
- No hard-coded per-plugin collection lists inside resolver logic.

### Phase D — Onboard acceptance
1. Apply accepted strip-side settings to onboard where relevant.
2. Validate parity for categories intended to affect onboard.

Acceptance criteria:
- Expected subset of settings affects onboard without regressions.

### Phase E — Persistence (after stabilization)
1. Freeze runtime schema.
2. Define per-output JSON structure.
3. Implement load/apply/save paths.

Acceptance criteria:
- Settings survive reboot and re-apply correctly by output/plugin context.

---

## Risks & Mitigations
- Risk: UI lockups from blocking callbacks.
  - Mitigation: keep callback execution outside LVGL lock (already enforced in recent fix).
- Risk: State coupling between plugin type and visible UI collections.
  - Mitigation: separate "visibility resolver" from "settings state store".
- Risk: Overlapping calibration controls causing unclear ownership.
  - Mitigation: define precedence rules early (requester command vs test UI overrides).

---

## Open Questions to Resolve Before Coding Persistence
- Final precedence model between requester-specified values and UI test overrides.
- Whether `z_offset` should support independent auto-advance rate in this phase or remain manual-only first.
- Exact boundary between "test-only controls" and "profile-ready controls".

---

## Recommended Immediate Next Step
Implement **Phase A** (manual x/y/z stepping collection and strip integration) with float-backed runtime values and long-session interaction testing before reintroducing walk windows.
