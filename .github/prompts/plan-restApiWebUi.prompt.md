## Plan: Modular REST API & Web UI Integration (Status & Next Steps)

This plan focuses on building a modular REST API for your ESP32 project, using X-Macro patterns for schema, and incrementally developing a web UI in sync with REST features.

### What We’ve Accomplished
1. **Modularized wifi_http_server.c**
   - Centralized handler registration, file serving, upload, and error handling.
   - Refactored for easy REST endpoint addition.

2. **Established X-Macro Schema Patterns**
   - io_rgb.def and rgb_plugins.def are single sources of truth for RGB command fields and plugin IDs/names.
   - Dispatcher header now uses X-Macro for source/target enums, ready for REST expansion.

3. **CMake/Include Path Modularization**
   - All .def files are accessible project-wide for REST and UI integration.

### Steps (Current & Next)
1. **Implement REST Endpoint Registration Table**
   - Use a central URI/command table in wifi_http_server.c for REST endpoints.
   - Register handlers using X-Macro data for future extensibility.

2. **Generate REST/JSON Schema from X-Macro .def Files**
   - Use X-Macro expansions to generate JSON schema for RGB commands and plugin options.
   - REST endpoint (e.g., /api/rgb_schema) serves this schema to the UI.

3. **Incremental Web UI Development**
   - As each REST endpoint is added, update the web UI to fetch schema/options and display controls (e.g., dropdown for animation, fields for parameters).
   - UI and firmware remain in sync via REST schema.

4. **Future REST Expansion**
   - Dispatcher and X-Macro patterns are ready for additional modules/endpoints as needed.

### Further Considerations
1. Confirm if you want to start with the RGB REST endpoint and schema generation next.
2. Would you like a sample REST handler and JSON schema generator for the RGB module?
3. UI will be developed incrementally alongside REST—confirm if you want UI scaffolding or just REST for now.
