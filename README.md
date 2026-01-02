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
│   │       ├── hsv_palette.*     # HSV palette and color pipeline
│   │       ├── noise_data.*      # Noise data for animation
│   │       ├── rgb_anim_*.c/h    # Animation plugin implementations
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
- **Modular Plugin System:** Easily add or remove hardware features (RGB, UART, USB, battery). All animation plugins and color requesters now use HSV (hue, saturation, value) for internal color representation, making color effects and transitions more intuitive and modular.
- **Dispatcher/Event-Task System:** Central dispatcher enables modular, event-driven or task-based feature integration
- **HSV-based Color Pipeline:** All color effects, plugins, and requesters use HSV (hue, saturation, value) for internal color representation. Conversion to RGB is performed centrally before hardware output, enabling smooth transitions and modular effect development.
- **RGB LED Animation System:** Modular RGB LED animation support with extensible animation plugins, now unified under the HSV color pipeline.
- **Battery/Fuel Gauge Support:** Direct support for battery voltage and fuel gauge monitoring (where available)
- **UART + LIDAR Integration:** Incremental path from UART echo to FreeRTOS task-based LIDAR data parsing
- **USB Serial Console:** USB support currently provides a serial console interface similar to UART
- **Kconfig-Driven Configuration:** Select board, features, and options via menuconfig
- **ESP-IDF Best Practices:** Modern build system, component manager, and FreeRTOS support
- **Interactive Palette Editor:** Jupyter notebook for designing, previewing, and exporting firmware-ready HSV palettes. See `main/plugins/rgb/resources/HSV_palette_editor.ipynb`.
- **Animation Palette System & Brightness Strategies:** Modular palette system with theme presets (Fire, Water, Lightning, Toxic, Aurora), tunable HSV curves, and multiple brightness mapping strategies (Index, Value, Value+Noise, etc.). See `palette_brightness_ranking.md` for details.
- **Noise-Driven Animation:** Uses Perlin and OpenSimplex2 noise for organic flicker, shimmer, and dynamic effects. Noise data and visualizations are in `main/plugins/rgb/resources/`.
- **Battery State Effects:** LED color and animation mapped to battery status, with clear, intuitive visual feedback. See `battery_state_effects.md` for mapping.

# Developer Notes

All color handling is now performed in HSV (hue, saturation, value, 0–255 range) format throughout the animation pipeline. Plugins and requesters use HSV, and conversion to RGB for hardware output is centralized in the RGB driver. This enables smooth color transitions, easier effect development, and a more modular animation system.

## Getting Started

### Prerequisites
- ESP-IDF v5.5.1 installed ([Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html))
- Supported Series D board (EdgeS3[D], TinyS3[D], FeatherS3[D], or ProS3[D])
- USB cable for programming

### Configure the Project

1. Set the target chip (e.g., ESP32-S3):

   ```sh
   idf.py set-target esp32s3
   ```

2. Open the configuration menu:

   ```sh
   idf.py menuconfig
   ```

   - In the "UM Series D Board Configuration" menu:
     - **Select your board type**
     - **Enable/disable features** (RGB, UART, USB, battery, etc.)
     - **Configure UART/LIDAR options** (UART port, baud rate, etc.)

### Build and Flash

1. Build, flash, and monitor:

   ```sh
   idf.py -p PORT flash monitor
   ```

   (Replace `PORT` with your serial port, e.g., COM3 or /dev/ttyUSB0)

2. To exit the monitor, type `Ctrl-]`.

## UART + LIDAR Development Path

- **Step 1:** UART echo example (verify communication)
- **Step 2:** Refactor to FreeRTOS task-based UART handling
- **Step 3:** Integrate RP-LIDAR A1M8 data parsing and processing
- **Step 4:** Extend for mapping, visualization, or robotics applications

## Animation & Palette System

- HSV-based palette system with 6-point curve editor for H, S, V channels (see palette editor notebook).
- Theme presets for rapid switching (Fire, Water, Lightning, Toxic, Aurora).
- Brightness mapping strategies: Index, Value, Value+Noise, and more. See `palette_brightness_ranking.md` for effect/brightness mapping.
- Noise-driven animation for natural flicker and shimmer (Perlin/OpenSimplex2).
- Battery state LED mapping: see `battery_state_effects.md` for how LED color/animation reflects battery status.

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

