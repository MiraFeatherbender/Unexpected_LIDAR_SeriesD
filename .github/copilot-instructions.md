# Copilot Instructions for New ESP-IDF UM Series D + UART LIDAR Project

## ESP-IDF UM Series D + UART LIDAR Project — AI Agent Guide

### ESP-IDF Version: v5.5.1 (not v5.5.2)

---

## Big Picture Architecture
- Modular ESP-IDF project targeting ESP32-family, with a core dispatcher system for decoupled inter-module communication.
- Main logic and subsystem plugins live in `main/plugins/`, organized by hardware IO (io_*.c/h) and subsystem folders (e.g., RPLIDAR, rgb, wifi).
- Each io_* module implements hardware IO (UART, GPIO, RGB, USB, Wi-Fi) and registers dispatcher handlers for message routing.
- Subsystem folders (e.g., RPLIDAR, rgb) contain higher-level logic, protocol parsing, and animation/state management, often bridging hardware IO and application logic.
- Board features (LED, GPIO, I2C, etc.) are abstracted in `src/` and configured via Kconfig (`Kconfig.projbuild`).
- LIDAR integration is incremental: start with UART echo, refactor to FreeRTOS tasks, then add LIDAR data parsing and protocol handling in RPLIDAR/.
- External dependencies managed via `idf_component.yml` and `managed_components/` (e.g., cJSON, TinyUSB, LED strip, mDNS).

## Developer Workflows
- **Configure features:** Use `idf.py menuconfig` to set board options (Kconfig).
- **Build:** Use ESP-IDF extension or `idf.py build` (root or `main/`).
- **Flash:** Use ESP-IDF extension or `idf.py flash` (auto-detects serial port).
- **Monitor:** Use ESP-IDF extension or `idf.py monitor` for UART output.
- **Clean:** Use `idf.py fullclean` to reset build artifacts.
- **Component management:** Update dependencies via `idf.py add-dependency` or edit `idf_component.yml`.
- **Debugging:** Use ESP-IDF monitor, check `sdkconfig` for config issues, and review FreeRTOS task logs for concurrency bugs.

## Project-Specific Conventions
- **Source structure:**
	- `main/` for app logic, `src/` for board abstraction, `examples/` for reference apps.
	- Use modular C files and keep feature logic isolated for easy extension.
- **Naming:**
	- Files: lower_case_with_underscores.
	- Functions: camelCase or ModuleName_FuncName (see dispatcher).
	- Macros: UPPER_CASE_WITH_UNDERSCORES.
- **FreeRTOS:**
	- Prefer task-based concurrency for UART/LIDAR data handling.
	- Use queues for inter-task communication (see dispatcher pattern).
- **Kconfig:**
	- Add new board features via `Kconfig.projbuild` and reference in CMakeLists.txt.
- **Component integration:**
	- Use `idf_component.yml` for external libraries (see cJSON, TinyUSB, LED strip).
	- Managed components live in `managed_components/`.

## Integration Points & Data Flow
- **UART/LIDAR:**
	- Start with echo example (see `main/`), then refactor to FreeRTOS tasks for non-blocking reads.
	- LIDAR parsing: buffer UART data, parse frames, and dispatch to mapping/visualization logic.
- **Board features:**
	- Abstracted in `src/`, enabled via Kconfig, and used in `main/` or `examples/`.
- **External libraries:**
	- cJSON for JSON parsing, TinyUSB for USB device support, LED strip for RGB control, mDNS for network discovery.

## Key Files & Directories
- `main/main.c`, `main/dispatcher.c/h`, `main/modules.def`: Main app and dispatcher/task logic.
- `src/`: Board abstraction modules.
- `examples/`: Reference apps (LED blink, color wheel, etc.).
- `Kconfig.projbuild`: Board feature config.
- `idf_component.yml`: External dependencies.
- `CMakeLists.txt` (root and `main/`): Build setup.
- `managed_components/`: External libraries.
- `README.md`: Project documentation and structure.

## Example Patterns
- **UART echo:** See `main/main.c` for initial UART setup and echo loop.
- **FreeRTOS task:** See `main/dispatcher.c` for task creation and message dispatch.
- **LED control:** See `examples/` for RGB LED usage.
- **Adding a new feature:**
	1. Add config to `Kconfig.projbuild`.
	2. Implement feature in `src/`.
	3. Reference in `main/` or `examples/`.

## Updating & Extending
- Keep code modular and document new features in `README.md`.
- When adding new board features, update Kconfig and CMakeLists.txt accordingly.
- For LIDAR: follow incremental path—UART echo → FreeRTOS tasks → LIDAR parsing.

---
*This project uses ESP-IDF v5.5.2. See https://aka.ms/vscode-instructions-docs for more info. Update this file as the codebase evolves.*


