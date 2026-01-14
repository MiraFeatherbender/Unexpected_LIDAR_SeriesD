# battery_json module

Provides dynamic battery RGB tier configuration via JSON file. Supports runtime reload and integration with io_battery dispatch system.

- JSON file: `/Battery_Levels.json` (root partition)
- Uses PSRAM for tier storage
- API: `battery_json_get_tiers()`, `battery_json_reload()`
- Handles plugin name mapping to RGB enum
- Designed for runtime reload via dispatch
