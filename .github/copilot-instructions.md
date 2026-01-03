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

---

# TEMPORARY MIGRATION PROJECT INSTRUCTIONS (APPEND-ONLY, SAFE TO REMOVE LATER)

I want to have a high‑level architectural discussion with you about my RGB animation system. 
Do NOT generate code unless I explicitly ask. 
Stay in discussion mode and help me reason about structure, modularity, and clean design.

CONTEXT YOU NEED:

1. I have a plugin‑based RGB animation system. 
   Each animation implements the interface defined in rgb_anim.h:
       begin()
       step(hsv_color_t *out)
       set_color(hsv_color_t hsv)
       set_brightness(uint8_t b)
   This interface must remain unchanged.

2. Some animations use HSV_palette.* (palette‑driven color identity).  
   Some animations use noise_data.* (noise‑driven temporal modulation).  
   Some use both.  
   I want to unify these into a **common system** so that future palette/noise animations share centralized logic.

3. The goal is to transform files like rgb_anim_fire.c into **tiny preset selectors**:
   - They choose which palette to use
   - They choose which noise fields to use
   - They choose the noise walk pattern
   - They choose the brightness strategy
   But they do NOT contain the core animation logic anymore.

4. All shared logic should be centralized in a new module called **rgb_anim_composer**.
   This module is responsible for:
       - sampling noise fields
       - applying walk specs
       - indexing palettes
       - applying brightness strategies
       - applying theme hue shift
       - producing the final HSV output
   Think of it as the “composition engine” for palette/noise‑based animations.

5. I want to expand the existing modules instead of creating unnecessary new ones:
   - Expand **noise_data.c/h** to include:
         NoiseField structs
         noise_field_init/value/step
         noise walk spec helpers
   - Expand **hsv_palette.c/h** to include:
         palette descriptors
         brightness strategy function pointers
         palette lookup helpers

6. The new **rgb_anim_composer** module should depend on:
       - noise_data (NoiseField + noise tables)
       - hsv_palette (palette descriptors + brightness strategies)
   Animation presets should depend on rgb_anim_composer, but rgb_anim_composer should NOT depend on animation presets.

7. I want to avoid scattering logic across many files. 
   Tuning an effect (fire, aurora, lightning, etc.) should be localized and intuitive. 
   The architecture should scale cleanly as I add more palette/noise‑based animations.

8. You have access to my entire codebase, so you can see how things are currently structured. 
   I want you to help me reason about:
       - where responsibilities should live
       - how to keep the architecture clean
       - how to avoid duplication
       - how to keep tuning simple
       - how to evolve the system without breaking existing animations

9. Again: do NOT generate code unless I explicitly ask. 
   Think like a systems architect who can see all my files.

Start by asking clarifying questions about my current architecture and constraints, 
and then help me explore the cleanest way to unify palette‑based and noise‑based animations under this new rgb_anim_composer system.
