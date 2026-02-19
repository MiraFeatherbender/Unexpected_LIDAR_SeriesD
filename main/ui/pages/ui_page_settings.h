#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "ui_input_adapter.h"
#include "ui_setting_item.h"

/**
 * Settings page template (vertical tiles):
 * - One setting per vertical tile
 * - Rotate navigates tiles when current setting is not in edit mode
 * - Tap (dir=0) toggles edit mode for current setting
 * - Int bars reuse ui_setting_item behavior from encoder_test
 */

typedef enum {
    UI_SETTING_TYPE_FLOAT_BAR,
    UI_SETTING_TYPE_INT_BAR,
    UI_SETTING_TYPE_ENUM_DROPDOWN,
} ui_setting_type_t;

/**
 * Single adjustable parameter descriptor
 */
typedef struct {
    const char *name;                          // Display name: "Frequency", "Noise Type", etc.
    ui_setting_type_t type;                    // Widget type
    
    // Range and current value
    float min_value;                           // For float/int bars
    float max_value;
    float current_value;
    
    // For enum dropdowns
    const char **enum_options;                 // Array of option strings
    int enum_count;                            // Number of enum options
    int current_enum_idx;                      // Current selection index
    
    // Callback when user changes value
    // on_change(collection_idx, setting_idx, new_value)
    void (*on_change)(int coll_idx, int set_idx, ui_setting_value_u *new_value);
} ui_setting_descriptor_t;

/**
 * Collection of related settings (e.g., all FastNoiseLite params)
 */
typedef struct {
    const char *collection_name;               // "FastNoiseLite", "RGB HSV", etc.
    ui_setting_descriptor_t *settings;         // Array of setting descriptors
    int settings_count;
} ui_setting_collection_t;

/**
 * Page instance state
 */
typedef struct {
    lv_obj_t *container;                       // Main page container
    lv_obj_t *tileview;                        // Vertical tileview for setting pages
    lv_obj_t **setting_tiles;                  // [settings_count] tile objects
    bool *setting_tile_loaded;                 // [settings_count] lazy-load state
    ui_setting_item_t *setting_items;          // [settings_count] int/float bar items
    lv_obj_t **enum_value_labels;              // [settings_count] enum value labels
    lv_obj_t **enum_name_labels;               // [settings_count] enum name labels
    bool enum_editing;                         // true when active enum label is in edit mode
    bool ignore_next_tap;                      // suppress first dir==0 after page show
    
    const ui_setting_collection_t *collection; // Current collection descriptor
    int active_setting_idx;                    // Index into collection->settings
} ui_page_settings_ctx_t;

/**
 * Derive encoder acceleration params from setting range
 * Can be used as a default implementation or overridden per collection
 */
void ui_setting_derive_accel_default(
    ui_setting_descriptor_t *setting,
    ui_encoder_accel_t *accel);

/**
 * Generic page init/show/hide/deinit for settings pages
 * Pass user_data = (void*)&ui_setting_collection_t to ui_page_t
 */
esp_err_t ui_page_settings_init(lv_obj_t *tile);
void ui_page_settings_show(lv_obj_t *tile);
void ui_page_settings_hide(void);
void ui_page_settings_deinit(void);

/**
 * Set the collection for the current page
 * Must be called before show()
 */
void ui_page_settings_set_collection(const ui_setting_collection_t *collection);
