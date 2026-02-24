# RGB Walk Spec (Float, Randomized Per Frame)

## Purpose
Define a shared walk-spec model for RGB plugins using float-based offsets in normalized noise space.

This replaces the older `uint8`-based walk increments with float semantics while preserving the existing behavior pattern: choose a random delta each frame from a bounded, quantized range.

Status note: as of current migration phase, this spec is deferred until dynamic/output instancing is finalized; current work keeps walk behavior compatibility-first.

---

## Coordinate / Scale Convention
- Target normalized domain: `[-1.0, 1.0]` per axis.
- Intended to align onboard and strip behavior under the same units.
- Useful conversion note from prior 8-bit pixel scale assumptions:
  - `1` step in `0..255` maps to `2/255 ~= 0.007843137` in `[-1,1]`.

---

## Walk Spec Semantics
For each axis (`x`, `y`, optional `z`), walk spec is:
- `min`: minimum frame delta (float)
- `max`: maximum frame delta (float)
- `step`: quantization increment (float)

Per frame:
1. Sample a random quantized delta in `[min, max]`.
2. Accumulate into axis offset:
   - `acc += delta` (or optionally `acc += delta * dt_seconds` for timing-independence)

This is a random-walk style update where deltas are re-sampled every frame.

---

## Quantized Random Delta Selection
Use integer index sampling over quantized bins:
- `n = round((max - min) / step)`
- `k = random integer in [0, n]`
- `delta = min + k * step`

Guidelines:
- Ensure `step > 0`.
- If `max < min`, swap or reject config.
- Clamp/re-normalize endpoint drift due to float precision as needed.

---

## Accumulator Handling
- Keep accumulators as `float`.
- Preferred domain handling: wrap in normalized domain to avoid unbounded drift.
  - Example conceptual behavior: wrap back into `[-1, 1]` when exiting bounds.
- Clamping is acceptable if plugin behavior requires hard limits, but wrapping is preferred for continuous traversal.

---

## Recommended Tuning Defaults
- Primary increment for tuning: `step = 0.005`
- Fine tuning increment: `step = 0.001`
- Start with small ranges (example): `min = -0.02`, `max = 0.02`

Interpretation:
- `0.005` is generally practical for visible movement without excessive jitter.
- `0.001` is useful for subtle drift/micro-adjustments.

---

## Notes for Refactor
- Keep this walk-spec contract shared across outputs where practical.
- FNL/plugin parameters will be recalibrated per plugin after migration, so strict backward numeric parity with legacy uint8 walk values is not required.
- This spec intentionally prioritizes consistency and tunability over legacy increment compatibility.
