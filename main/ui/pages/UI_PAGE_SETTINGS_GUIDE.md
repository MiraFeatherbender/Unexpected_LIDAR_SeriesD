# Settings Page Template Guide

## Overview

`ui_page_settings` is a reusable page template for adjusting collections of related settings via a **dropdown selector + value widget** interface.

**Layout:**
```
[Setting Name ▼]
[████░░░░] 65.2
```

- **Dropdown:** Selects which setting to adjust (shows current setting name)
- **Value Widget:** Either a bar (float/int) or dropdown (enum) for adjusting the selected setting

## Creating a New Settings Page

### 1. Define Setting Descriptors

```c
static ui_setting_descriptor_t my_settings[] = {
    {
        .name = "Parameter 1",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.0f,
        .max_value = 100.0f,
        .current_value = 50.0f,
        .on_change = my_on_change_callback,
    },
    {
        .name = "Parameter 2",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"Option A", "Option B", "Option C"},
        .enum_count = 3,
        .current_enum_idx = 0,
        .on_change = my_on_change_callback,
    },
};
```

### 2. Create Setting Collection

```c
static const ui_setting_collection_t my_collection = {
    .collection_name = "My Settings",
    .settings = my_settings,
    .settings_count = sizeof(my_settings) / sizeof(my_settings[0]),
    .derive_accel = NULL,  // Use default, or implement custom derivation
};
```

### 3. Implement Change Callback

```c
static void my_on_change_callback(int coll_idx, int set_idx, ui_setting_value_t *new_value)
{
    // Update internal state based on setting index
    if (set_idx == 0) {
        // Parameter 1 changed to new_value->f
        actual_api_set_param1(new_value->f);
    } else if (set_idx == 1) {
        // Parameter 2 changed to new_value->i (enum index)
        actual_api_set_param2(new_value->i);
    }
    
    // Post dispatcher message if needed
    // dispatcher_pool_send_ptr(...);
}
```

### 4. Wrap Page Lifecycle

```c
static esp_err_t my_page_init(lv_obj_t *tile)
{
    ui_page_settings_set_collection(&my_collection);
    return ui_page_settings_init(tile);
}

static void my_page_show(lv_obj_t *tile)
{
    ui_page_settings_show(tile);
}

static void my_page_hide(void)
{
    ui_page_settings_hide();
}

static void my_page_deinit(void)
{
    ui_page_settings_deinit();
}

const ui_page_t ui_page_my_settings = {
    .id = UI_PAGE_ID_MY_SETTINGS,  // Define in ui_page.h
    .name = "My Settings",
    .init = my_page_init,
    .show = my_page_show,
    .hide = my_page_hide,
    .deinit = my_page_deinit,
};
```

### 5. Register in pages.def

```
X_PAGE(my_settings)
```

## Setting Types

### FLOAT_BAR
Continuous float adjustment. Renders as a bar widget.

```c
{
    .name = "Frequency",
    .type = UI_SETTING_TYPE_FLOAT_BAR,
    .min_value = 0.001f,
    .max_value = 0.1f,
    .current_value = 0.01f,
    .on_change = on_change,
}
```

### INT_BAR
Continuous integer adjustment. Renders as a bar widget.

```c
{
    .name = "Seed",
    .type = UI_SETTING_TYPE_INT_BAR,
    .min_value = 0,
    .max_value = 10000,
    .current_value = 1337,
    .on_change = on_change,
}
```

### ENUM_DROPDOWN
Discrete enum selection. Renders as a dropdown.

```c
{
    .name = "Noise Type",
    .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
    .enum_options = (const char*[]){"Option A", "Option B", "Option C"},
    .enum_count = 3,
    .current_enum_idx = 0,
    .on_change = on_change,
}
```

## Encoder Acceleration Derivation

By default, encoder acceleration parameters are automatically derived from each setting's range:

- **base_step:** 0.001 for floats, 1 for integers
- **gain_k:** Scales inversely with range (small ranges = fast, large ranges = slow)
- **tau_ms:** 250ms (constant)
- **accel_max:** 2.0 + (range / 1000)

### Custom Derivation

Implement a custom derivation function for more control:

```c
static void my_derive_accel(ui_setting_descriptor_t *setting, ui_encoder_accel_t *accel)
{
    // Custom logic...
    accel->base_step = 0.01f;
    accel->gain_k = 0.05f;
    accel->tau_ms = 300.0f;
    accel->accel_max = 1.0f;
}

static const ui_setting_collection_t my_collection = {
    .collection_name = "My Settings",
    .settings = my_settings,
    .settings_count = ...,
    .derive_accel = my_derive_accel,  // Custom derivation
};
```

## Examples

See:
- `ui_page_settings_fnl_example.c` - FastNoiseLite settings page
- `ui_page_encoder_test.c` - Original single-setting page (reference)
