#include "ui_page.h"
#include "ui_pages.h"
#include "ui_page_settings.h"
#include "ui_dispatch_bridge.h"
#include "io_rgb_led_fnl_contract.h"
#include "dispatcher.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include <string.h>

/**
 * Example settings page for FastNoiseLite parameters
 * 
 * Shows how to:
 * - Define setting descriptors for a collection
 * - Implement on_change callbacks for setting updates
 * - Register as a concrete page using the generic template
 */

static const char *TAG = "ui_fnl_page";

typedef enum {
    FNL_SET_NOISE_TYPE = 0,
    FNL_SET_FREQUENCY,
    FNL_SET_SEED,
    FNL_SET_ROTATION_3D,
    FNL_SET_FRACTAL_TYPE,
    FNL_SET_OCTAVES,
    FNL_SET_LACUNARITY,
    FNL_SET_GAIN,
    FNL_SET_WEIGHTED_STRENGTH,
    FNL_SET_PING_PONG_STRENGTH,
    FNL_SET_CELLULAR_DISTANCE,
    FNL_SET_CELLULAR_RETURN,
    FNL_SET_CELLULAR_JITTER,
    FNL_SET_DOMAIN_WARP_TYPE,
    FNL_SET_DOMAIN_WARP_AMP,
} fnl_setting_index_t;

static fnl_state s_fnl_ui_state;
static uint32_t s_fnl_ui_version = 0;
static int64_t s_last_push_us = 0;

static esp_err_t fnl_send_ctx_sync(rgb_led_fnl_ctx_t *ctx, TickType_t timeout);
static esp_err_t fnl_get_snapshot(fnl_state *out_state);
static esp_err_t fnl_push_snapshot(bool force_send);
static void fnl_sync_descriptors_from_state(void);
static void fnl_update_state_from_change(int set_idx, const ui_setting_value_u *new_value);
static void fnl_on_change(int coll_idx, int set_idx, ui_setting_value_u *new_value);

/**
 * FastNoiseLite setting descriptors bound to fnl_state
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
        .min_value = -2147483648.0f,
        .max_value = 2147483647.0f,
        .current_value = 1337,
        .on_change = fnl_on_change,
    },
    {
        .name = "Rotation 3D",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"None", "Improve XY", "Improve XZ"},
        .enum_count = 3,
        .current_enum_idx = 0,
        .on_change = fnl_on_change,
    },
    {
        .name = "Fractal Type",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"None", "FBm", "Rigid", "PingPong", "DomainWarpProgressive", "DomainWarpIndependent"},
        .enum_count = 6,
        .current_enum_idx = 0,
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
    {
        .name = "Ping Pong",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.0f,
        .max_value = 10.0f,
        .current_value = 2.0f,
        .on_change = fnl_on_change,
    },
    {
        .name = "Cell Dist",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"Euclidean", "EuclideanSq", "Manhattan", "Hybrid"},
        .enum_count = 4,
        .current_enum_idx = 1,
        .on_change = fnl_on_change,
    },
    {
        .name = "Cell Return",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"CellValue", "Distance", "Distance2", "Dist2Add", "Dist2Sub", "Dist2Mul", "Dist2Div"},
        .enum_count = 7,
        .current_enum_idx = 1,
        .on_change = fnl_on_change,
    },
    {
        .name = "Cell Jitter",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.0f,
        .max_value = 2.0f,
        .current_value = 1.0f,
        .on_change = fnl_on_change,
    },
    {
        .name = "Warp Type",
        .type = UI_SETTING_TYPE_ENUM_DROPDOWN,
        .enum_options = (const char*[]){"OpenSimplex2", "OpenSimplex2Reduced", "BasicGrid"},
        .enum_count = 3,
        .current_enum_idx = 0,
        .on_change = fnl_on_change,
    },
    {
        .name = "Warp Amp",
        .type = UI_SETTING_TYPE_FLOAT_BAR,
        .min_value = 0.0f,
        .max_value = 5.0f,
        .current_value = 1.0f,
        .on_change = fnl_on_change,
    },
};

static const ui_setting_collection_t fnl_collection = {
    .collection_name = "FastNoiseLite",
    .settings = fnl_settings,
    .settings_count = sizeof(fnl_settings) / sizeof(fnl_settings[0]),
};

static esp_err_t fnl_send_ctx_sync(rgb_led_fnl_ctx_t *ctx, TickType_t timeout)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (!sem) return ESP_ERR_NO_MEM;
    ctx->sem = sem;

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = TARGET_RGB_LED;

    dispatcher_pool_send_params_t params = {
        .type = DISPATCHER_POOL_STREAMING,
        .source = SOURCE_OLED_INDEV,
        .targets = targets,
        .data = NULL,
        .data_len = 0,
        .context = ctx,
    };

    esp_err_t err = ui_dispatch_bridge_send_params(&params);
    if (err != ESP_OK) {
        vSemaphoreDelete(sem);
        ctx->sem = NULL;
        return err;
    }

    if (xSemaphoreTake(sem, timeout) != pdTRUE) {
        vSemaphoreDelete(sem);
        ctx->sem = NULL;
        return ESP_ERR_TIMEOUT;
    }

    vSemaphoreDelete(sem);
    ctx->sem = NULL;
    return ESP_OK;
}

static esp_err_t fnl_get_snapshot(fnl_state *out_state)
{
    if (!out_state) return ESP_ERR_INVALID_ARG;

    rgb_led_fnl_ctx_t ctx = {
        .op = RGB_LED_FNL_OP_GET_SNAPSHOT,
        .version = 0,
        .state_size = sizeof(fnl_state),
        .state = out_state,
        .sem = NULL,
        .status = RGB_LED_FNL_OK,
        .user_data = NULL,
    };

    esp_err_t err = fnl_send_ctx_sync(&ctx, pdMS_TO_TICKS(250));
    if (err != ESP_OK) return err;

    return (ctx.status == RGB_LED_FNL_OK) ? ESP_OK : ESP_FAIL;
}

static esp_err_t fnl_push_snapshot(bool force_send)
{
    int64_t now_us = esp_timer_get_time();
    if (!force_send && (now_us - s_last_push_us) < 50000) {
        return ESP_OK;
    }

    uint32_t next_version = s_fnl_ui_version + 1;
    if (next_version == 0) {
        next_version = 1;
    }

    rgb_led_fnl_ctx_t ctx = {
        .op = RGB_LED_FNL_OP_SET_SNAPSHOT,
        .version = next_version,
        .state_size = sizeof(fnl_state),
        .state = &s_fnl_ui_state,
        .sem = NULL,
        .status = RGB_LED_FNL_OK,
        .user_data = NULL,
    };

    esp_err_t err = fnl_send_ctx_sync(&ctx, pdMS_TO_TICKS(250));
    if (err != ESP_OK) return err;

    if (ctx.status != RGB_LED_FNL_OK) {
        return ESP_FAIL;
    }

    s_fnl_ui_version = next_version;
    s_last_push_us = now_us;
    return ESP_OK;
}

static void fnl_sync_descriptors_from_state(void)
{
    fnl_settings[FNL_SET_NOISE_TYPE].current_enum_idx = (int)s_fnl_ui_state.noise_type;
    fnl_settings[FNL_SET_FREQUENCY].current_value = s_fnl_ui_state.frequency;
    fnl_settings[FNL_SET_SEED].current_value = (float)s_fnl_ui_state.seed;
    fnl_settings[FNL_SET_ROTATION_3D].current_enum_idx = (int)s_fnl_ui_state.rotation_type_3d;
    fnl_settings[FNL_SET_FRACTAL_TYPE].current_enum_idx = (int)s_fnl_ui_state.fractal_type;
    fnl_settings[FNL_SET_OCTAVES].current_value = (float)s_fnl_ui_state.octaves;
    fnl_settings[FNL_SET_LACUNARITY].current_value = s_fnl_ui_state.lacunarity;
    fnl_settings[FNL_SET_GAIN].current_value = s_fnl_ui_state.gain;
    fnl_settings[FNL_SET_WEIGHTED_STRENGTH].current_value = s_fnl_ui_state.weighted_strength;
    fnl_settings[FNL_SET_PING_PONG_STRENGTH].current_value = s_fnl_ui_state.ping_pong_strength;
    fnl_settings[FNL_SET_CELLULAR_DISTANCE].current_enum_idx = (int)s_fnl_ui_state.cellular_distance_func;
    fnl_settings[FNL_SET_CELLULAR_RETURN].current_enum_idx = (int)s_fnl_ui_state.cellular_return_type;
    fnl_settings[FNL_SET_CELLULAR_JITTER].current_value = s_fnl_ui_state.cellular_jitter_mod;
    fnl_settings[FNL_SET_DOMAIN_WARP_TYPE].current_enum_idx = (int)s_fnl_ui_state.domain_warp_type;
    fnl_settings[FNL_SET_DOMAIN_WARP_AMP].current_value = s_fnl_ui_state.domain_warp_amp;
}

static void fnl_update_state_from_change(int set_idx, const ui_setting_value_u *new_value)
{
    if (!new_value) return;

    switch ((fnl_setting_index_t)set_idx) {
        case FNL_SET_NOISE_TYPE:
            s_fnl_ui_state.noise_type = (fnl_noise_type)new_value->i32;
            break;
        case FNL_SET_FREQUENCY:
            s_fnl_ui_state.frequency = new_value->f32;
            break;
        case FNL_SET_SEED:
            s_fnl_ui_state.seed = new_value->i32;
            break;
        case FNL_SET_ROTATION_3D:
            s_fnl_ui_state.rotation_type_3d = (fnl_rotation_type_3d)new_value->i32;
            break;
        case FNL_SET_FRACTAL_TYPE:
            s_fnl_ui_state.fractal_type = (fnl_fractal_type)new_value->i32;
            break;
        case FNL_SET_OCTAVES:
            s_fnl_ui_state.octaves = new_value->i32;
            break;
        case FNL_SET_LACUNARITY:
            s_fnl_ui_state.lacunarity = new_value->f32;
            break;
        case FNL_SET_GAIN:
            s_fnl_ui_state.gain = new_value->f32;
            break;
        case FNL_SET_WEIGHTED_STRENGTH:
            s_fnl_ui_state.weighted_strength = new_value->f32;
            break;
        case FNL_SET_PING_PONG_STRENGTH:
            s_fnl_ui_state.ping_pong_strength = new_value->f32;
            break;
        case FNL_SET_CELLULAR_DISTANCE:
            s_fnl_ui_state.cellular_distance_func = (fnl_cellular_distance_func)new_value->i32;
            break;
        case FNL_SET_CELLULAR_RETURN:
            s_fnl_ui_state.cellular_return_type = (fnl_cellular_return_type)new_value->i32;
            break;
        case FNL_SET_CELLULAR_JITTER:
            s_fnl_ui_state.cellular_jitter_mod = new_value->f32;
            break;
        case FNL_SET_DOMAIN_WARP_TYPE:
            s_fnl_ui_state.domain_warp_type = (fnl_domain_warp_type)new_value->i32;
            break;
        case FNL_SET_DOMAIN_WARP_AMP:
            s_fnl_ui_state.domain_warp_amp = new_value->f32;
            break;
        default:
            break;
    }
}

static void fnl_on_change(int coll_idx, int set_idx, ui_setting_value_u *new_value)
{
    (void)coll_idx;
    if (!new_value) return;
    if (set_idx < 0 || set_idx >= (int)(sizeof(fnl_settings) / sizeof(fnl_settings[0]))) return;

    fnl_update_state_from_change(set_idx, new_value);
    if (fnl_push_snapshot(false) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to push FNL snapshot");
    }
}

/**
 * Page lifecycle wrappers that delegate to generic settings template
 */
static esp_err_t fnl_page_init(lv_obj_t *tile)
{
    s_fnl_ui_state = fnlCreateState();
    s_fnl_ui_version = 0;
    s_last_push_us = 0;

    if (fnl_get_snapshot(&s_fnl_ui_state) != ESP_OK) {
        ESP_LOGW(TAG, "Using local defaults, snapshot fetch failed");
    }
    fnl_sync_descriptors_from_state();

    ui_page_settings_set_collection(&fnl_collection);
    return ui_page_settings_init(tile);
}

static void fnl_page_show(lv_obj_t *tile)
{
    ui_page_settings_show(tile);
}

static void fnl_page_hide(void)
{
    if (fnl_push_snapshot(true) != ESP_OK) {
        ESP_LOGW(TAG, "Final FNL snapshot push failed");
    }
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
