# Unexpected LIDAR Series D ESP-IDF Project

**Target Platform:** ESP32-family (ESP-IDF v5.5.2)

This project provides a modular ESP-IDF workspace for Unexpected Maker Series D boards, with a focus on board abstraction, feature modularity, and UART-based RP-LIDAR A1M8 integration. It is designed for incremental development, learning, and extension, and is implemented entirely in C via ESP-IDF.

## Supported Boards (Pin Count may limit feature use)
- TinyS3[D] 
- FeatherS3[D] (No RGB supprt)
- ProS3[D] (ProS3[D] is the board in use for this build)

## File & Naming Conventions

Short, discoverable naming rules to keep the repository organized and easy to navigate:

- Use lower_case_with_underscores for file and symbol names (C files, headers, and resources).
- Prefixes indicate intent and location:
  - `io_` — hardware I/O modules (GPIO, UART, I2C, PWM, sensors). Typically live under `main/plugins/` and provide concrete hardware access and dispatcher callbacks.
  - `mod_` — pure software modules or logical processing units (state machines, aggregators) that are independent of hardware.
  - `rgb_`, `wifi_`, `lidar_`, etc. — subsystem prefixes for files belonging to a specific subsystem/folder; keep these grouped under a shared folder (e.g., `plugins/rgb/`, `plugins/wifi/`).
  - `dispatcher_*`, `dispatcher_pool*` — core dispatcher and pointer pool code.
  - `*_test.c` or `*_self_test.c` — test and self-test harnesses.
- Keep public APIs in headers (`.h`) and implementations in `.c` files; put module-level configs in `Kconfig.projbuild` where appropriate.
- Follow the `dispatcher_module.h` contract/conventions for modules that register with the dispatcher.

## Project Structure

```
.
├── main/
│   ├── dispatcher.c/.h               # Event/task dispatcher for modular plugins
│   ├── dispatcher_module.h           # Module helpers & compatibility shims
│   ├── dispatcher_pool.c/.h          # Pointer-pool allocator for compact messages
│   ├── dispatcher_pool_config.json   # pool sizing and tuning
│   ├── plugins/                      # Modular hardware feature plugins
│   │   ├── battery/                  # Battery module and monitor
│   │   │   ├── io_battery.c/.h
│   │   ├── io_rgb.c/.h               # RGB LED and animations (modular)
│   │   ├── io_gpio.c/.h              # GPIO helpers, button/line sensor handling
│   │   ├── io_lidar.c/.h             # RP-LIDAR integration and coordinator
│   │   ├── RPLIDAR/                  # LIDAR parsing helpers & message builders
│   │   │   ├── lidar_coordinator.c/.h
│   │   │   ├── lidar_message_builder.*
│   │   ├── io_ultrasonic.c/.h        # Ultrasonic sensor (RMT) — ping/echo processing
│   │   ├── io_fatfs.c/.h             # FAT/SPIFFS helpers (USB MSC)
│   │   ├── io_log.c/.h               # Central log helpers (console/SSE integration)
│   │   ├── io_usb_msc.c/.h           # USB MSC / serial console
│   │   ├── io_wifi_ap.c/.h           # Wi-Fi AP and SSE server
│   │   ├── dispatcher_allocator.c/.h # pool allocator config helper
│   │   ├── dispatcher_pool_test.c/.h # test harness for pointer pool
│   │   ├── mod_line_sensor_window.c/.h # line sensor window module
│   │   ├── rgb/                      # RGB LED animation implementations
│   │   │   ├── hsv_palette.*         # HSV palette descriptors, lookup, brightness strategies
│   │   │   ├── noise_data.*          # Noise field descriptors, noise walk helpers
│   │   │   ├── rgb_anim_composer.*   # Central palette/noise animation engine
│   │   │   └── resources/            # Palette editor, docs, and noise assets
│   │   ├── wifi/                     # HTTP/SSE server and static resources
│   │   │   ├── wifi_http_server.*    # HTTP file server (serves FATFS files)
│   │   │   ├── wifi_sse.*            # Server-Sent Events streaming helpers
│   │   │   └── resources/            # Development/backup of FATFS-stored files served by HTTP/SSE
│   ├── src/
│   │   ├── UMSeriesD_idf.c/.h        # C board abstraction for Series D
│   ├── Kconfig.projbuild             # Project-specific configuration options
│   ├── idf_component.yml             # ESP-IDF component manager dependencies
│   └── main.c                        # Main application entry point
├── docs/                             # Design notes and references
│   ├── AnkleBiter2.md                # Project overview and roadmap
│   ├── io_ultrasonic.md
│   ├── io_heading_control.md
│   ├── io_encoder.md                 # moved to docs/.references for authoritative copy
│   └── .references/                  # hardware references and LIDAR docs (PDFs, schematics)
├── CMakeLists.txt                    # Root build system configuration
├── sdkconfig                         # Project configuration (auto-generated)
└── README.md                         # Project documentation (this file)
```


## Key Features
- **UM Series D Board Abstraction:** Clean C wrappers for board features (LEDs, battery, GPIO, etc.)
- **Modular Plugin System:** Easily add or remove hardware features (RGB, UART, USB, battery, LIDAR, ultrasonic). Plugins follow a consistent dispatcher-module contract for lifecycle & message handling.
- **Dispatcher/Event-Task System:** Central dispatcher enables modular, event-driven or task-based feature integration and pointer-pool messages for low-latency data paths.
- **Runtime JSON Configuration:** Use JSON files (SPIFFS/FAT) for live, versioned runtime config; Kconfig remains available for build-time defaults and hardware constraints.
- **Diagnostics & Telemetry:** `/diag` JSON endpoint and SSE telemetry planned/available for health, memory (DRAM/IRAM/PSRAM), task stack watermarks, and module status.
- **Motor Control (LEDC + PID):** LEDC for PWM, safe direction-change sequencing, S‑curve / jerk limiting, and per-wheel PID control (planned/scaffolded).
- **Encoder Support (PCNT / ISR):** Default PCNT up/down mode (A pulse, B direction), with optional ISR+LUT for 4× quadrature decoding and better instant direction detection.
- **Ultrasonic (RMT):** RMT-based ultrasonic driver (1 tick ≈ 1 cm by default) for obstacle proximity messages, already bench-tested on breadboard.
- **LIDAR Integration:** RP-LIDAR UART integration and a coordinator for parsing and forwarding LIDAR data to modules.
- **Heading & Steering Control:** Separate angular controller (θ_goal → ω_cmd) and linear velocity controller combined via differential kinematics; publishes heading & Δθ for UI display.
- **RGB Animation System:** HSV-based color pipeline, unified palette/noise composer, Jupyter palette editor, and modular animation presets.
- **Battery Monitoring & Visual Mapping:** Battery state mapping to LED effects and system alerts; override options available for indication reuse.
- **ESP-IDF Best Practices:** Modern build system, component manager, FreeRTOS, and sensible task/ISR structure.
- **Interactive Tools & Documentation:** Palette editor notebook and in-repo docs (`docs/` and `docs/.references/`) covering ultrasonic, encoder, motor, heading designs and hardware references (RPLIDAR docs and MCU schematic).





## Getting Started

### Prerequisites
- ESP-IDF v5.5.2 ([Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html))
- Supported Series D board (EdgeS3[D], TinyS3[D], FeatherS3[D], or ProS3[D])
- USB cable for programming

### Configure the Project

1. Set the target chip (e.g., ESP32-S3):

   ```sh
   idf.py set-target esp32s3
   idf.py menuconfig
   ```

   - In the "UM Series D Board Configuration" menu:
     - **Select your board type**
     - **Configure UART/LIDAR options** (UART port, baud rate, etc.)

### Build and Flash

1. Build, flash, and monitor:

   ```sh
   idf.py -p PORT flash monitor
   ```
   (Replace `PORT` with your serial port, e.g., COM3 or /dev/ttyUSB0)
   - To exit the monitor, type `Ctrl-]`.

## Sensor & Vehicle Development Path

- **Step 1:** UART echo example (verify communication)
- **Step 2:** Refactor to FreeRTOS task-based UART handling
- **Step 3:** Integrate RP-LIDAR A1M8 data parsing and processing
- **Step 4:** Add ultrasonic RMT driver and verify ping/echo distances
- **Step 5:** Bench-test encoders with synthetic quadrature generator (PCNT)
- **Step 6:** Implement LEDC motor PWM + safe direction control + per-wheel PID
- **Step 7:** Heading controller (θ_goal → ω_cmd), combine with v_cmd → wheel setpoints
- **Step 8:** Integrate avoidance and light-weight mapping (VFH) using LIDAR + ultrasonic
- **Step 9:** Expose telemetry & control via web UI (SSE) and diagnostics endpoint


## Animation & Palette System

- All palette/noise-based animations use a single, configuration-driven engine (`rgb_anim_composer`). Animation files are just presets—no duplicated logic.
- Palette lookup, noise sampling, walk, and brightness strategies are handled centrally for consistency and easy tuning.
- Create or tune effects by adjusting configuration or descriptors—no need to rewrite animation logic.
- Use the palette editor notebook for HSV curve design; see resources for noise data and effect mapping.
- Battery status effects use the same unified system.
- Basic animations (solid, blink, fade) remain independent and use their own logic.


## Documentation
A set of design and implementation notes are available under `docs/` and `docs/.references/`:
- `docs/AnkleBiter2.md` — project overview and roadmap
- `docs/io_ultrasonic.md` — ultrasonic design and decisions
- `docs/io_encoder.md` — encoder/PCNT design and tests
- `docs/io_motor_control.md` — motor control, PID, and safe sequencing
- `docs/io_heading_control.md` — heading math and UI telemetry

**Note:** RPLIDAR core documentation, MCU pinout, and schematic are located in `docs/.references/`.


## Troubleshooting
- Double-check board and feature selection in menuconfig
- Verify pin numbers and hardware connections
- Use `idf.py fullclean` if configuration changes are not taking effect
- Check serial monitor for error messages
- Check the `/diag` endpoint (when available) for memory, task, and module health information
- Verify runtime JSON config (expected path: `/spiffs/config.json`) if behavior differs from runtime expectations
- Consult the [ESP-IDF Troubleshooting Guide](https://docs.espressif.com/projects/esp-idf/en/latest/troubleshooting.html)

## Status & Roadmap
- Candle flame animation: Complete
- Palette editor: Complete
- Ultrasonic (RMT): Hardware bench-tested (breadboard)
- Encoder (PCNT): Test harness & integration planned (synthetic generator available)
- Motor control (LEDC + PID): Design docs and skeleton ready; implementation planned
- JSON runtime config: Design and loader planned (persisted on SPIFFS)
- Diagnostics endpoint & SSE telemetry: Planned (will include memory regions and task stack watermarks)
- LIDAR integration: In development

## License

See [LICENSE](../LICENSE) for details.

