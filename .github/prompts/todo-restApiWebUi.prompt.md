# TODO List: Modular REST API & Web UI Integration

1. **Finalize modular REST scaffolding**
   - Ensure wifi_http_server.c is modular, with centralized handler registration and error handling. Confirm it is ready for REST endpoint integration using X-Macro patterns.
   - Status: completed

2. **Implement REST endpoint registration table**
   - Design and implement a central URI/command table in wifi_http_server.c that can register REST endpoints using X-Macro data. Prepare for dynamic handler addition.
   - Status: in-progress

3. **Generate REST/JSON schema from X-Macro .def files**
   - Use X-Macro patterns (io_rgb.def, rgb_plugins.def) to generate both C structs and REST/JSON schema. REST endpoint should serve plugin list and parameter schema as JSON.
   - Status: not-started

4. **Integrate REST schema with web UI**
   - Update the web UI to fetch and use the REST/JSON schema for dynamic dropdowns and parameter controls, ensuring UI is always in sync with firmware.
   - Status: not-started

5. **Prepare for future REST expansion**
   - Dispatcher and X-Macro patterns are ready for additional modules/endpoints as needed.
   - Status: not-started
