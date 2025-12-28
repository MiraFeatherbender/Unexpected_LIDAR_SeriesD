
# Copilot Instructions for New ESP-IDF UM Series D + UART LIDAR Project

**ESP-IDF Version: v5.5.1**

- ESP-IDF v5.5.1 project targeting ESP32-family boards, based on UM Series D hardware abstraction.
- Integrates selected source files and examples for board features (e.g., RGB LED, GPIO, I2C).
- Adds a development track for RP-LIDAR A1M8 integration via UART.
- Designed for modular, incremental learning and extension.

## Goals
### General Project Goals
- Provide a minimal, clean ESP-IDF setup focused on UM Series D board support.
- Enable easy configuration of board features using Kconfig and modular source structure.
- Serve as a flexible starting point for further development and experimentation.

### UART + LIDAR Goals
- Establish reliable UART communication between ESP32 and RP-LIDAR A1M8.
- Start with basic UART echo functionality, then expand to FreeRTOS task-based data handling.
- Parse and process LIDAR data streams for mapping, visualization, or robotics applications.
- Maintain a modular codebase for incremental learning and feature integration.

## Key Files & Structure
- `src/` : Core source files for UM Series D board abstraction.
- `examples/` : Selected example applications (e.g., LED blink, color wheel).
- `Kconfig.projbuild` : Project-specific configuration options.
- `idf_component.yml` : ESP-IDF component manager dependencies.
- `CMakeLists.txt` : Build setup (root and main/).
- `LICENSE` : Project license.
- `.gitignore` : Ignore build artifacts and other non-source files.

## Usage Pattern
- Configure board and feature options via menuconfig (Kconfig).
- Build and flash using ESP-IDF workflow.
- For UART/LIDAR: Start with echo example, refactor to FreeRTOS tasks, then integrate LIDAR parsing.

- Familiarize yourself with ESP-IDF v5.5.1 build and configuration workflow.
- Review UM Series D abstraction and example usage.
- For UART/LIDAR, follow the incremental development path: echo → tasks → LIDAR integration.
- Maintain modularity and clarity in code changes.
- Document new features and usage in README.md as the project evolves.

---
*This project is specifically using ESP-IDF v5.5.1. Edit this file to update project-specific AI agent instructions. See https://aka.ms/vscode-instructions-docs for more info.*
