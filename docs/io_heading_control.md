# Heading & Angular Control — Design, Display, and Calculations 🧭

This document complements the motor control and encoder docs and describes how to compute heading, control angular motion, and publish UI-friendly telemetry for a differential‑drive vehicle.

---

## Goals 🎯
- Compute accurate heading changes over each control interval (Δθ) and cumulative heading.  
- Provide a robust heading controller (PID → ω_cmd) separate from linear speed control (v_cmd).  
- Combine angular and linear commands into wheel setpoints for differential drive.  
- Publish concise UI payloads for real‑time display (heading needle, heading rate, target vs actual).  

---

## Kinematics & core equations 🔢
- Convert wheel linear velocities (v_l, v_r in m/s) into vehicle linear/angular velocities:
  - v = (v_r + v_l)/2
  - ω = (v_r - v_l)/b  (b = track width in meters)
- Heading change over a time slice dt:
  - Δθ = ω * dt  (radians)
- Update cumulative heading (wrap to [-π, π] or [0, 2π]):
  - θ_new = wrap(θ_old + Δθ)

Optional: turning radius and steering metaphor
- Instantaneous turning radius: R = v / ω (if ω ≠ 0)  
- Ackermann steering angle approximation (for UI steering wheel metaphor): φ = atan(L / R) with vehicle wheelbase L (or compute curvature κ = ω / v)

---

## Heading controller architecture 🧭
1. Measure θ_current — maintain cumulative heading or compute from Δθ accumulation using encoder-derived ω.  
2. Compute heading error:
   - err = wrap_angle(θ_goal - θ_current)  // ensure shortest path in [-π, π]
3. Heading PID → output ω_cmd (rad/s):
   - ω_cmd = PID_heading(err)
   - Apply output limits: |ω_cmd| ≤ ω_max
   - Use deadband: if |err| < eps_ang then ω_cmd = 0 to avoid chatter
4. Optionally prioritize steering over forward motion:
   - v_cmd_effective = v_cmd * f(|err|)  // f reduces forward velocity when heading error large
5. Combine into wheel setpoints:
   - v_r = v_cmd_effective + (ω_cmd * b / 2)
   - v_l = v_cmd_effective - (ω_cmd * b / 2)
6. Enforce per‑wheel speed limits (scale v_r/v_l uniformly if needed) and feed wheel setpoints into per‑wheel controllers.

Notes on wrap_angle:
```c
static inline float wrap_angle(float a) {
  while (a <= -M_PI) a += 2*M_PI;
  while (a >  M_PI) a -= 2*M_PI;
  return a;
}
```

---

## Discrete-time implementation tips ⏱️
- Run heading loop at deterministic cadence (e.g., 50–200 Hz); use vTaskDelayUntil.  
- Compute θ update using central difference or direct ω measurement from wheels for more stability if available.  
- Apply small low‑pass filtering to measured ω and v to reduce noise before integrating or feeding PID.  

---

## UI & telemetry payloads (recommended) 🖥️
Publish a compact binary or JSON payload via the dispatcher for the web UI (update each control tick or at UI rate):

Example JSON payload (sent to SSE / TARGET_SSE or TARGET_LOG):
```json
{
  "timestamp_ms": 1620000000000,
  "theta": 1.234,           // radians, wrapped
  "theta_deg": 70.7,       // convenience
  "theta_goal": 1.5708,
  "theta_err_deg": 20.3,
  "omega_cmd": 0.12,       // rad/s
  "omega_meas": 0.10,      // rad/s
  "delta_theta_deg": 0.57, // last-slice heading change in degrees
  "v_cmd": 0.25,           // m/s
  "v_meas": 0.23           // m/s
}
```

UI suggestions:
- Heading needle (actual vs target) with small smoothing.  
- Numeric dispays: θ_goal, θ_actual, heading error (deg).  
- Angular velocity (deg/s) and recent Δθ for animation.  
- Visualize turning radius or steering angle as optional indicator.

---

## Tuning & behavior choices 🔧
- Tune heading PID with v_cmd = 0 (rotate in place) until angular response meets requirements, then enable both controllers together.  
- Use deadband on heading error (e.g., 1–3°) to avoid small oscillations causing UI jitter.  
- If heading accuracy must be prioritized, scale v_cmd down while |err| large to allow turning-in-place.  
- For smoother UI, low‑pass theta and omega measurements before sending to web UI (but keep raw values for safety logging).

---

## Testing & validation 🧪
- Unit: simulate v_r/v_l → Δθ math to validate mapping and wrap handling.  
- Bench: run heading-only test: set θ_goal offset, expect θ to converge with minimal overshoot.  
- Integration: set combined v_cmd + θ_goal and verify the vehicle follows while respecting limits.  
- UI: check cross-browser SSE streaming at intended rate (e.g., 10–20 Hz for smooth needle) and ensure no jitter.

---

## Example helper interface (pseudo C)
```c
typedef struct {
  float theta;        // radians
  float theta_goal;   // radians
  float omega_cmd;    // rad/s
  float v_cmd;        // m/s
  float delta_theta;  // rad in last slice
} heading_t;

void heading_update(heading_t *h, float v_l, float v_r, float dt);
void heading_control_step(heading_t *h, float v_user, float theta_goal, float dt);
```

---

If you want, I can add a small `heading_helper.c/h` to the repo implementing `wrap_angle`, `heading_update`, `heading_control_step`, and an example that publishes the JSON payload via the dispatcher. Want that scaffolded next? 🚀