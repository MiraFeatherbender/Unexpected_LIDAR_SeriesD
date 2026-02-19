#include "ui_page.h"
#include "ui_pages.h"
#include "ui_page_settings.h"
#include "esp_err.h"
#include "lvgl.h"

/**
 * Example settings page for FastNoiseLite parameters
 * 
 * Shows how to:
 * - Define setting descriptors for a collection
 * - Implement on_change callbacks for setting updates
 * - Register as a concrete page using the generic template
 */

// Forward declaration of callbacks
static void fnl_on_change(int coll_idx, int set_idx, ui_setting_value_u *new_value);

/**
 * Placeholder FastNoiseLite setting descriptors
 * In a real implementation, these would call actual FNL functions
 */
static ui_setting_descriptor_t fnl_settings[] = {
    {
        .name = "Noise Type",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"OpenSimplex2", "OpenSimplex2S", "Cellular", "Perlin", "ValueCubic", "Value"},
        .enum_count = 6,
        .current_enum_idx = 0,
        .on_change = fnl_on_change,
    },
    {
        .name = "Frequency",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.001f,
        .max_value = 8.0f,
        .current_value = 0.01f,
        .on_change = fnl_on_change,
    },
    {
        .name = "Seed",
        .type = UI_SETTING_TYPE_INT_BAR,
        .min_value = 0,
        .max_value = 10000,
        .current_value = 1337,
        .on_change = fnl_on_change,
    },
    {
        .name = "Fractal Type",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"None", "FBm", "Rigid", "PingPong", "DomainWarpProgressive", "DomainWarpIndependent"},
        .enum_count = 6,
        .current_enum_idx = 1,
        .on_change = fnl_on_change,
    },
    {
        .name = "Octaves",
        .type = UI_SETTING_TYPE_INT_BAR,
        .min_value = 1,
        .max_value = 8,
        .current_value = 3,
        .on_change = fnl_on_change,
    },
    {
        .name = "Lacunarity",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 1.0f,
        .max_value = 4.0f,
        .current_value = 2.0f,
        .on_change = fnl_on_change,
    },
    {
        .name = "Gain",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.0f,
        .max_value = 1.0f,
        .current_value = 0.5f,
        .on_change = fnl_on_change,
    },
    {
        .name = "Weighted Strength",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.0f,
        .max_value = 1.0f,
        .current_value = 0.0f,
        .on_change = fnl_on_change,
    },
};

static const ui_setting_collection_t fnl_collection = {
    .collection_name = "FastNoiseLite",
    .settings = fnl_settings,
    .settings_count = sizeof(fnl_settings) / sizeof(fnl_settings[0]),
};

/**
 * Callback for any FastNoiseLite setting change
 * In a real implementation, this would:
 * - Call the appropriate FNL setter function
 * - Post a dispatcher message to notify other modules
 */
static void fnl_on_change(int coll_idx, int set_idx, ui_setting_value_u *new_value)
{
    if (set_idx >= (int)(sizeof(fnl_settings) / sizeof(fnl_settings[0]))) return;
    
    ui_setting_descriptor_t *setting = &fnl_settings[set_idx];
    
    // TODO: Call actual FNL functions based on setting name
    // fnl_set_frequency(new_value->f32);
    // fnl_set_seed(new_value->i32);
    // etc.
    
    // TODO: Post dispatcher message to notify other modules
    // dispatcher_pool_send_ptr(...);
}

/**
 * Page lifecycle wrappers that delegate to generic settings template
 */
static esp_err_t fnl_page_init(lv_obj_t *tile)
{
    ui_page_settings_set_collection(&fnl_collection);
    return ui_page_settings_init(tile);
}

static void fnl_page_show(lv_obj_t *tile)
{
    ui_page_settings_show(tile);
}

static void fnl_page_hide(void)
{
    ui_page_settings_hide();
}

static void fnl_page_deinit(void)
{
    ui_page_settings_deinit();
}

/**
 * Page descriptor for registration in pages.def
 * Example entry for pages.def:
 * 
 *   X_PAGE(SETTINGS_FNL)
 */
const ui_page_t ui_page_SETTINGS_FNL = {
    .id = UI_PAGE_SETTINGS_FNL,
    .name = "FastNoise",
    .init = fnl_page_init,
    .show = fnl_page_show,
    .hide = fnl_page_hide,
    .deinit = fnl_page_deinit,
};
