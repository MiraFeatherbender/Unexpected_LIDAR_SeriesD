# Unexpected LIDAR Series D ESP-IDF Project

**Target Platform:** ESP32-family (ESP-IDF v5.5.1)

This project provides a modular ESP-IDF workspace for Unexpected Maker Series D boards, with a focus on board abstraction, feature modularity, and UART-based RP-LIDAR A1M8 integration. It is designed for incremental development, learning, and extension, and is implemented entirely in C via ESP-IDF.

## Supported Boards
- TinyS3[D]
- FeatherS3[D]
- ProS3[D] (ProS3[D] is the board in use for this build)

## Project Structure

```
.
├── main/
│   ├── dispatcher.c/.h           # Event/task dispatcher for modular plugins
│   ├── plugins/                  # Modular hardware feature plugins
│   │   ├── io_battery.*          # Battery voltage/fuel gauge support
│   │   ├── io_rgb.*              # RGB LED and animations (modular)
│   │   ├── io_uart0.*            # UART0 abstraction (for LIDAR, etc.)
│   │   ├── io_usb.*              # USB device/host support
│   │   └── rgb/                  # RGB LED animation implementations
│   │       ├── hsv_palette.*     # HSV palette descriptors, lookup, brightness strategies
│   │       ├── noise_data.*      # Noise field descriptors, noise walk helpers
│   │       ├── rgb_anim_composer.* # Central palette/noise animation engine (unified logic)
│   │       ├── rgb_anim_*.c/h    # Animation preset selectors (config only)
│   │       └── resources/        # Palette editor, docs, and noise assets
│   │           ├── HSV_palette_editor.ipynb      # Interactive palette editor (Jupyter)
│   │           ├── palette_brightness_ranking.md # Effect/brightness mapping docs
│   │           ├── battery_state_effects.md      # Battery state LED mapping
│   │           ├── openSimplex2_data.h           # OpenSimplex2 noise data
│   │           ├── perlin_noise_data.h           # Perlin noise data
│   │           ├── openSimplex2.bmp              # Noise visualization
│   │           └── perlinNoise.bmp               # Noise visualization
│   ├── src/
│   │   ├── UMSeriesD_idf.*       # C board abstraction for Series D
│   ├── Kconfig.projbuild         # Project-specific configuration options
│   ├── idf_component.yml         # ESP-IDF component manager dependencies
│   └── main.c                    # Main application entry point
├── CMakeLists.txt                # Root build system configuration
├── sdkconfig                     # Project configuration (auto-generated)
└── README.md                     # Project documentation (this file)
```


## Key Features
- **UM Series D Board Abstraction:** Clean C wrappers for board features (LEDs, battery, etc.)
- **Modular Plugin System:** Easily add or remove hardware features (RGB, UART, USB, battery). All animation plugins and color requesters use HSV (hue, saturation, value) for internal color representation, making color effects and transitions more intuitive and modular.
- **Dispatcher/Event-Task System:** Central dispatcher enables modular, event-driven or task-based feature integration
- **HSV-based Color Pipeline:** All color effects, plugins, and requesters use HSV (hue, saturation, value) for internal color representation. Conversion to RGB is performed centrally before hardware output, enabling smooth transitions and modular effect development.
- **Unified Palette/Noise Animation Engine:** All palette/noise-based animations are now configuration-driven and use a single, unified engine (`rgb_anim_composer`). The core logic for palette lookup, noise sampling, walk patterns, and brightness strategies is centralized, ensuring consistency and reducing code duplication.
- **Config-Driven Animation Presets:** Animation files (e.g., `rgb_anim_fire.c`) are now minimal preset selectors. They only specify which palette, noise field, walk spec, and brightness strategy to use—no animation logic is duplicated. New effects are created by providing a config to the composer, not by writing new logic.
- **Extensible Palettes & Noise:** Palettes and noise fields are described in `hsv_palette.*` and `noise_data.*`, with helpers for lookup, walk, and brightness. Tuning and new effects are achieved by changing configuration, not code.
- **Battery/Fuel Gauge Support:** Direct support for battery voltage and fuel gauge monitoring (where available)
- **UART + LIDAR Integration:** Incremental path from UART echo to FreeRTOS task-based LIDAR data parsing
- **USB Serial Console:** USB support currently provides a serial console interface similar to UART
- **Kconfig-Driven Configuration:** Select board, features, and options via menuconfig
- **ESP-IDF Best Practices:** Modern build system, component manager, and FreeRTOS support
- **Interactive Palette Editor:** Jupyter notebook for designing, previewing, and exporting firmware-ready HSV palettes. See `main/plugins/rgb/resources/HSV_palette_editor.ipynb`.
- **Animation Palette System & Brightness Strategies:** Modular palette system with theme presets (Fire, Water, Lightning, Toxic, Aurora), tunable HSV curves, and multiple brightness mapping strategies (Index, Value, Value+Noise, etc.). See `palette_brightness_ranking.md` for details.
- **Noise-Driven Animation:** Uses Perlin and OpenSimplex2 noise for organic flicker, shimmer, and dynamic effects. Noise data and visualizations are in `main/plugins/rgb/resources/`.
- **Battery State Effects:** LED color and animation mapped to battery status, with clear, intuitive visual feedback. See `battery_state_effects.md` for mapping.




## Getting Started

### Prerequisites
- ESP-IDF v5.5.1 ([Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html))
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

## UART + LIDAR Development Path

- **Step 1:** UART echo example (verify communication)
- **Step 2:** Refactor to FreeRTOS task-based UART handling
- **Step 3:** Integrate RP-LIDAR A1M8 data parsing and processing
- **Step 4:** Extend for mapping, visualization, or robotics applications


## Animation & Palette System

- All palette/noise-based animations use a single, configuration-driven engine (`rgb_anim_composer`). Animation files are just presets—no duplicated logic.
- Palette lookup, noise sampling, walk, and brightness strategies are handled centrally for consistency and easy tuning.
- Create or tune effects by adjusting configuration or descriptors—no need to rewrite animation logic.
- Use the palette editor notebook for HSV curve design; see resources for noise data and effect mapping.
- Battery status effects use the same unified system.
- Basic animations (solid, blink, fade) remain independent and use their own logic.

## Example Output

You should see log output for enabled features (e.g., RGB LED status, UART data, LIDAR packets) in the serial monitor. Example:

```text
I (300) main: RGB LED initialized
I (500) main: UART0 RX: 0xA5 0x5A ...
I (1200) main: LIDAR packet received, angle=123.4, distance=567mm
```

## Troubleshooting
- Double-check board and feature selection in menuconfig
- Verify pin numbers and hardware connections
- Use `idf.py fullclean` if configuration changes are not taking effect
- Check serial monitor for error messages
- Consult the [ESP-IDF Troubleshooting Guide](https://docs.espressif.com/projects/esp-idf/en/latest/troubleshooting.html)

## Status & Roadmap
- Candle flame animation: Complete
- Palette editor: Complete
- Battery monitor effects: Planned/in progress
- LIDAR integration: In development

## License

See [LICENSE](../LICENSE) for details.

