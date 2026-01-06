# USB + FATFS Modular Architecture Plan

## Overview
This document describes the modular architecture for integrating USB CDC, USB MSC, FATFS file access, and runtime LUT/image loading in the ESP32 project. The design leverages the dispatcher system for clean, decoupled communication between modules.

---

## Modules & Responsibilities

### 1. FATFS Module
- Handles all file read/write operations using ESP-IDF FATFS APIs.
- Routes file access requests and responses through the dispatcher system.
- Can be enabled/disabled by messages from the MSC module to prevent concurrent access with the host PC.

### 2. MSC (Mass Storage Class) Module
- Controls USB MSC interface, exposing the FATFS partition as a USB drive to the host PC.
- Sends messages via dispatcher to enable/disable FATFS module access (ensuring safe, exclusive access).
- Can be triggered by a button or command.

### 3. BMP Decoder Module
- Receives file data (e.g., BMP image) from FATFS module via dispatcher.
- Decodes BMP data and sends processed LUT/data to the RAM loader module.

### 4. RAM Loader Module
- Receives decoded LUT/data and loads it into RAM for runtime use.
- Used for noise tables, PID LUTs, or other large, swappable datasets.

### 5. RGB LED/Battery Module
- Listens for MSC mode messages to override battery status animation with MSC mode animation.
- Returns to normal battery animation when MSC mode is disabled.

---

## Workflow Example
1. **Boot:**
    - FATFS module loads LUTs/images, routes data to BMP decoder, then to RAM loader.
2. **MSC Mode Enabled:**
    - MSC module disables FATFS access and exposes the partition to the PC.
    - RGB LED shows MSC mode animation.
3. **MSC Mode Disabled:**
    - FATFS module is re-enabled for firmware access.
    - RGB LED returns to battery status animation.

---

## Dispatcher System
- All modules communicate via dispatcher messages for clean decoupling.
- Easy to add new file types, decoders, or data loaders in the future.

---

## Benefits
- Safe, exclusive access to FATFS for both firmware and host PC.
- Flexible, swappable LUT/image management.
- Scalable and maintainable architecture for future features.

---

## Future Extensions
- Add support for other file formats (PNG, CSV, etc.)
- Implement additional USB classes (HID, MIDI, etc.)
- Add more runtime data loaders or decoders as needed.
