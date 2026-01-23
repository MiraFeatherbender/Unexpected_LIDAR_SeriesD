# Motor Control — Goals, Design Intent, and Decisions 🔧

**Scope:** PWM speed control for a dual H‑bridge motor driver IC (driver handles internal FETs). Integrates encoder feedback (PCNT/ISR) for closed‑loop velocity control and safe motor sequencing.

---

## Goals 🎯
- Provide robust, deterministic velocity control (PID) using encoder feedback.  
- Keep CPU load low and behavior predictable; rely on hardware peripherals where appropriate (LEDC for PWM, PCNT for counting).  
- Ensure safe transitions for direction changes and stopping (no shoot‑through, minimal mechanical shock).  
- Make parameters configurable (PWM freq/resolution, PID loop rate, slew, deadband).

---

## High‑level architecture 🏗️
- PWM generator: **LEDC** (recommended) — drives PWM input of motor IC.  
  - Optional: **MCPWM** if complementary outputs or dead‑time are required by hardware (not needed for integrated driver ICs).  
- Direction control: two GPIOs per motor (IN1/IN2) with encoding: 00=coast, 01=fwd, 10=rev, 11=brake (optional).  
- Velocity sensing: encoders via **PCNT** (default) or ISR+LUT quadrature for 4× decoding.  
- Control loop: velocity PID running at fixed cadence (e.g., 50–200 Hz; default 100 Hz) with anti‑windup, output saturation, and slew limiting for PWM.

---

## Key decisions & rationale ✅
- Use **LEDC** because the motor driver accepts PWM + direction logic and LEDC is simple and low overhead. Use a fixed PWM frequency (e.g., 16 kHz) to avoid audible noise.  
- Use **PCNT up/down** with A=pulse and B=direction as default for encoder counts (low CPU, robust). Provide an ISR/LUT option for 4× resolution if needed.  
- Tie PID sign to direction: compute signed velocity error → sign selects IN1/IN2, magnitude maps to PWM duty (absolute value).  
- Implement **safe direction-change procedure**: set duty=0 (or ramp down), wait short interval, change direction bits atomically, ramp up duty (slew rate limit).  
- Implement an S‑curve / jerk limiting approach via acceleration/jerk constraints for smooth ramps and reduced mechanical stress.

---

## Control loop details ⚙️
- Sampling / control rate: default **100 Hz** (dt = 0.01 s). Use vTaskDelayUntil for deterministic cadence.  
- PID loop: tune P → I → D (small I, D optional); apply anti‑windup (clamp integrator when output saturated).  
- S‑curve integration (discrete example):
  - Acc_cmd = controller_output (clamped to ±amax)
  - vel += Acc_cmd * dt
  - duty = map(|vel|) to PWM duty (clamped)
  - direction = sign(vel) with deadband hysteresis around zero
- Deadband and hysteresis: define a small velocity window (e.g., ±5 units) to force direction=0 and avoid chatter.  
- PWM mapping: use feedforward term (k_ff) + PID correction to improve response.

---

## Direction & mode mapping (per motor) 🟦
- 2 control GPIOs per motor: `IN1`, `IN2`
  - `00` = coast
  - `01` = forward
  - `10` = reverse
  - `11` = brake (optional; may be replaced by software braking)
- Update both motors’ control bits together using atomic set/clear (GPIO W1TS/W1TC) or a small critical section.

---

## Safety & robustness 🛡️
- Command watchdog: set PWM=0 (or brake) if commands stop for configured timeout.  
- Direction change interlock: always reduce duty to 0 before changing IN1/IN2 (wait safety_ms).  
- Limit integrator and apply slew limits on duty changes.  
- Tune PCNT filter and Schmitt thresholds on encoder inputs to avoid noise artifacts.

---

## Configuration items (Kconfig suggestions) ⚙️
- `MOTOR_PWM_FREQ_HZ` (default 16000)  
- `MOTOR_PWM_RES_BITS` (default 10–12)  
- `MOTOR_CONTROL_LOOP_HZ` (default 100)  
- `MOTOR_MAX_ACCEL` / `MOTOR_MAX_JERK`  
- `MOTOR_ENCODER_PPR`, `MOTOR_GEAR_RATIO`  
- `MOTOR_DIRECTION_DEADBAND`  

---

## Testing & validation plan ✅
1. Bench test PWM & direction GPIO with LEDs/test loads (verify safe sequences).  
2. Verify encoder + PCNT using synthetic quadrature generator (software/RMT) before connecting real encoders.  
3. With real encoders, validate counts and sign at various speeds; tune filter/hysteresis.  
4. Tune PID: start with P, then add I, then D if necessary. Validate under load & edge cases (stall, sudden setpoint change).  
5. Test watchdog and emergency stop behaviors.

---

## Example pseudocode (control loop)

```c
// constants: dt, deadband, k_ff, max_duty
while (1) {
  vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100)); // 100Hz

  int counts = read_and_clear_encoder();
  float vel_meas = counts_to_velocity(counts, dt);
  float vel_err = vel_cmd - vel_meas;

  // PID -> acc_cmd (clamped)
  acc_cmd = pid_update(vel_err, dt);
  acc_cmd = clamp(acc_cmd, -amax, amax);

  vel_state += acc_cmd * dt; // integrate

  if (fabsf(vel_state) <= deadband) {
    set_direction(COAST);
    set_pwm_duty(0);
  } else {
    int dir = (vel_state > 0) ? FORWARD : REVERSE;
    // safe direction change: ramp to 0, switch pins, ramp up
    set_direction_safe(dir);
    duty = map_velocity_to_duty(fabsf(vel_state));
    set_pwm_duty(duty);
  }
}
```

---

## Next steps I can take
- Add a **motor_control** plugin scaffold: LEDC setup, safe direction helper, PID loop skeleton, Kconfig entries and a simple test app (PCNT + synthetic generator).  
- Or supply a focused **LEDC + direction helper** file and a reusable `set_direction_atomic()` helper.

Which would you like me to prepare next? ✨
