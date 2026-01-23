# AnkleBiter2 — Project Overview

⚡ AnkleBiter2 is a compact differential‑drive robot platform focused on responsive line following, obstacle avoidance (LIDAR + ultrasonic), closed‑loop motor control, and simple mapping/heading control. This document summarizes architecture, module responsibilities, design decisions, configuration strategy, diagnostic approach, and short‑term priorities.

---

## Goals
- Reliable line following with reactive obstacle avoidance (LIDAR + ultrasonic).  
- Robust velocity & steering control using motor drivers + encoder feedback.  
- Lightweight mapping / avoidance (e.g., vector field histogram) and real‑time telemetry to a web UI.  
- Clean extensible module architecture and runtime configurability via JSON.

---

## System Overview
- MCU: ESP32‑S3 based board (UM Series D).  
- Peripherals:
  - LIDAR (RPLIDAR integration) — streaming & coordinator modules.  
  - Ultrasonic (RMT receiver/transmit) — obstacle ping/echo.  
  - Quadrature encoders (A/B Hall) — PCNT default with optional ISR/LUT 4× mode.  
  - Motor control: dual H‑bridge driver IC (PWM + IN1/IN2 direction).  
  - RGB LED, battery monitor, line sensors, optional OLED (future).

---

## Key Design Decisions
- **Dispatcher:** central message bus (pointer messages) for low‑latency intermodule comms; lightweight module contract in `main/dispatcher_module.h`.  
- **Runtime config:** primary config via JSON files (persisted on SPIFFS/FAT) for live tuning; Kconfig remains for build time defaults/fallbacks.  
- **Encoders:** default PCNT up/down (A pulse, B direction); provide ISR+LUT option for 4× decoding and diagnostics.  
- **Motor control:** LEDC for PWM, safe direction-change sequence, S‑curve ramps and per‑wheel PID (sampled at ~100 Hz).  
- **Heading control:** separate angular PID (θ_goal → ω_cmd) and linear speed controller (v_cmd) combined via differential kinematics.  
- **Ultrasonic:** RMT at ~58.333 µs tick (1 tick ≈ 1 cm) for simple integer mapping, optional 1 µs mode available.  
- **Diagnostics:** `/diag` JSON endpoint and SSE telemetry for health, memory, stack high‑water, and module status.  

---

## Documents & Design Notes
- Ultrasonic design: `docs/io_ultrasonic.md`  
- Encoder & PCNT design: `docs/.references/io_encoder.md`  
- Motor control (PID, LEDC, safe sequencing): `docs/.references/io_motor_control.md`  
- Heading/Angular control + UI telemetry: `docs/io_heading_control.md`  
- LIDAR-specific notes and hardware references (RPLIDAR core docs, MCU pinout and schematic) are in `docs/.references`

---

## Module Responsibilities (high level)
- `io_lidar`: RX/TX, parsing, and coordinator (`plugins/RPLIDAR`).  
- `io_ultrasonic`: RMT TX/RX, publish distance messages.  
- `io_encoder`: encoder read (PCNT/ISR), publish wheel velocities.  
- `io_motor_control`: LEDC + direction helpers, PID loops, safe interfaces.  
- `io_rgb`, `io_battery`, `io_gpio`, `io_log`: utility, status and visual feedback.

---

## Telemetry & Web UI
- Use SSE for real‑time UI updates (10–20 Hz). Send compact JSON payloads with `timestamp`, `theta`, `theta_goal`, `v_cmd`, `v_meas`, `battery`, `errors`.  
- Diagnostics endpoint `/diag` returns health + memory usage per region + task stack high water marks.

---

## Configuration Strategy
- JSON schema (versioned) stored at `/spiffs/config.json` with backup file and loader/validator.  
- Expose runtime editing via web UI and persist on demand. Kconfig used for board defaults and compile constraints.

---

## Safety & Reliability
- Safe direction change: always decelerate (duty→0), change direction bits atomically, then ramp to new duty.  
- Watchdogs: command watchdog per motor and a global system watchdog for stalled modules.  
- Input filtering: PCNT filter for encoders; Schmitt‑trigger on Hall/sine signals (SN74HC14 or SN74LVC2G14 recommended).

---

## Testing & Validation Plan
- Unit math tests for wrap_angle, Δθ, and kinematic mappings.  
- Synthetic hardware tests: software quadrature generator + PCNT harness, RMT ultrasonic emulator, LEDC PWM stub tests.  
- Integration tests on hardware: encoder count validation, PID step response, heading convergence, obstacle avoidance scenarios.

---

## Short-term Sprint (3–5 tasks)
1. Ultrasonic plugin RMT integration & basic test (hardware on your bench).  
2. JSON config loader + validation + SPIFFS persistence.  
3. Diagnostic endpoint `/diag` + SSE health telemetry (include memory regions & task stack water marks).  
4. PCNT test harness and integration with motor control loop (safe direction + PID skeleton).  
5. Expand dispatcher module contract to include lifecycle hooks and health check function (drives uniformity and extensibility).

---

## Future Enhancements
- Basic SLAM/vector field histogram obstacle avoidance refinement and map visualization in web UI.  
- Optional OLED for in-field diagnostics & control.  
- CI & hardware-in-the-loop tests for critical loops (PID & encoder behavior).  

---

## Where to find things
- Code: `main/` and `main/plugins/`  
- Docs: `docs/` and `docs/.references/`  
- Config file template: add `/spiffs/config.json.example` (to be created in sprint)

