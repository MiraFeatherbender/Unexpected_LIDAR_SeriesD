# Encoder (AB‑Phase Hall) — Design, Goals, and Decisions 🔧

**Summary**
- Purpose: measure motor speed and direction using AB‑phase Hall encoders on the motor shaft and publish velocity through the project's dispatcher (pointer pool). For your hardware: **12 PPR** on motor shaft, **1:90** gear, motor up to ~113 RPM unloaded (design for headroom to higher RPM).
- Primary approach (default): **PCNT up/down mode** with A as the pulse input and B as the direction/control input (low CPU, robust).
- Optional high‑resolution/low‑latency mode: **ISR + LUT** quadrature decoding for true 4× resolution and immediate per‑edge updates.

---

## Goals 🎯
- Reliable velocity and direction readings for closed‑loop control.  
- Low CPU usage and predictable timing during normal operation.  
- Simple hardware to convert analog/Hall outputs into clean digital A/B signals.  
- Easy testability (synthetic generator) and robust against noise/jitter.

---

## Hardware interface — signal conditioning 🧩
- Encoder outputs: two 90° phase sine/Hall signals. These must be converted to clean logic levels before feeding ESP32S3 pins.
- Preferred solution: **CMOS Schmitt‑trigger** (e.g., **SN74HC14** DIP or **SN74LVC2G14** SOT‑23 breakout). Advantages: built‑in hysteresis, cheap, easy to breadboard.
- If amplitude/offset require it: **AC‑couple + bias** or a dedicated comparator with adjustable hysteresis.
- Route conditioned A → PCNT pulse pin; conditioned B → PCNT control pin.

Hardware checklist:
- 0.1 µF decoupling on Schmitt supply
- series input resistor (≈100 Ω) for protection
- optional AC coupling cap (100 nF) + Vcc/2 bias divider if signal is not centered

---

## PCNT architecture & configuration (recommended) ⚙️
- Use PCNT in *pulse+control* (up/down) mode:
  - pulse_gpio = A (edge counts)
  - ctrl_gpio = B (sampled at edges to decide increment/decrement)
  - pos_mode = COUNT_INC, neg_mode = COUNT_DIS (or COUNT_INC for 2×)
  - hctrl_mode / lctrl_mode configured so B=HIGH → increment (test & invert if needed)
  - enable input filter (`pcnt_filter_set`) to reject short glitches
- Sampling: periodically read & clear PCNT (e.g., every **T = 100 ms**), convert counts → motor RPM and wheel RPM.

Math (example):
- counts = read_and_clear()
- motor_revs = counts / pulses_per_rev
- motor_rpm = motor_revs * (60.0 / T)
- wheel_rpm = motor_rpm / gear_ratio
- For your defaults: pulses_per_rev = 12, gear_ratio = 90, T = 0.1 s

Pros:
- Very low CPU usage, hardware filtering, reliable at high sustained rates.
Cons:
- Typical 1× (or 2×) resolution unless you add extra logic; not per‑edge instantaneous like ISR.

---

## ISR + LUT quadrature decoding (optional high‑res mode) ⚡
- Attach interrupts to both edges of A and B (or both edges of both pins). On each edge:
  1. Read current A and B levels.
  2. Build 4‑bit index: idx = (prev_state << 2) | curr_state.
  3. delta = quad_table[idx] (16‑entry table returning +1, −1 or 0).
  4. prev_state = curr_state; atomic add(delta) to counter.
- Provides **4× resolution** (counts every transition) and immediate direction.
- Must be minimal ISR (IRAM_ATTR, no blocking, atomic ops). For your expected worst case (~10.8k edges/s per motor), two motors are easily handled on the S3.

When to use:
- If you require per‑edge responsiveness or maximum resolution (4×PPR), or must detect invalid transitions.

---

## Testing & validation plan ✅
1. Use a single‑probe oscilloscope to verify A and B separately:
   - Confirm peak‑to‑peak amplitude, offset, shape, and phase (≈90°).
2. If signals look clean, wire Schmitt buffers and test logic outputs.
3. Software bench test (before real encoder): implement a synthetic quadrature generator (GPIO toggles or RMT) to validate PCNT and read loop (forward & reverse).
4. With the real encoder: run motor forward/reverse, sample PCNT for T (100 ms) and check counts vs expected; adjust filter and polarity if needed.
5. Edge cases: test at extremes, check for chatter, and tune filter or hysteresis.

Small synthetic generator idea:
- Sequence states: (A=1,B=0) -> (1,1) -> (0,1) -> (0,0) at controlled timing to emulate forward motion.
- Reverse by reversing the sequence.

---

## Integration & messaging 📡
- Publish velocity samples via `dispatcher_pool_send_ptr_params`:
  - Target: `TARGET_ENCODER_LEFT` / `TARGET_ENCODER_RIGHT` (or `TARGET_ENCODER` with id field)
  - Payload: small struct with encoder_id, counts, motor_rpm_x100, wheel_rpm_x100, direction
- Default publish interval: same as sample interval (100 ms).

Example payload (binary, little endian):
```c
struct encoder_msg {
  uint8_t id; // 0 = left, 1 = right
  int32_t counts; // signed counts in the interval
  int32_t motor_rpm_x100;
  int32_t wheel_rpm_x100;
};
```

---

## Edge cases & robustness 🛡️
- If A/B edges are near-simultaneous sometimes, comparator hysteresis and PCNT filter will help; ISR approach can detect & flag invalid transitions.
- Use longer sample intervals or smoothing (EWMA, median) to improve low‑speed resolution.
- Consider enabling a watchdog or stall detection if counts drop unexpectedly.

---

## Next steps I can take for you
- Scaffold a **PCNT test module** (synthetic quadrature generator + PCNT config + sample task) for bench validation.  
- Draft minimal **IRAM ISR + LUT** reference snippet (no integration) for 4× decoding.  
- Add a small `Kconfig` block for encoder pins, sample interval, pulses_per_rev and gear_ratio.

Which of those would you like me to generate next? 🔧