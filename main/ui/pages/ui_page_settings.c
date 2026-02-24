#include "ui_page_settings.h"
#include "ui_setting_item.h"
#include "ui_styles.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ui_page_settings";

// Global context for the active settings page
static ui_page_settings_ctx_t *s_page_ctx = NULL;
static const ui_setting_collection_t *s_pending_collection = NULL;

static void on_encoder_input(int8_t dir);
static int clamp_index(int idx, int minv, int maxv);
static void create_setting_tile_widget(ui_page_settings_ctx_t *ctx, int idx);
static void unload_setting_tile_widget(ui_page_settings_ctx_t *ctx, int idx);

static const char *get_enum_option_text(const ui_setting_descriptor_t *setting, int idx)
{
    if (!setting || !setting->enum_options || setting->enum_count <= 0) return "";
    idx = clamp_index(idx, 0, setting->enum_count - 1);
    const char *text = setting->enum_options[idx];
    return text ? text : "";
}

static void update_enum_value_label(ui_page_settings_ctx_t *ctx, int idx)
{
    if (!ctx || !ctx->collection || !ctx->enum_value_labels) return;
    if (idx < 0 || idx >= ctx->collection->settings_count) return;

    ui_setting_descriptor_t *setting = &ctx->collection->settings[idx];
    if (setting->type != UI_SETTING_TYPE_ENUM_DROPDOWN) return;

    lv_obj_t *value_label = ctx->enum_value_labels[idx];
    if (!value_label) return;

    lv_label_set_text(value_label, get_enum_option_text(setting, setting->current_enum_idx));
}

static void set_enum_value_marquee(ui_page_settings_ctx_t *ctx, int idx, bool enabled)
{
    if (!ctx || !ctx->collection || !ctx->enum_value_labels) return;
    if (idx < 0 || idx >= ctx->collection->settings_count) return;

    lv_obj_t *value_label = ctx->enum_value_labels[idx];
    if (!value_label) return;

    lv_label_set_long_mode(value_label, enabled ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR : LV_LABEL_LONG_MODE_CLIP);
}

static void set_enum_value(ui_page_settings_ctx_t *ctx, int idx, int option_idx, bool notify)
{
    if (!ctx || !ctx->collection) return;
    if (idx < 0 || idx >= ctx->collection->settings_count) return;

    ui_setting_descriptor_t *setting = &ctx->collection->settings[idx];
    if (setting->type != UI_SETTING_TYPE_ENUM_DROPDOWN) return;

    option_idx = clamp_index(option_idx, 0, setting->enum_count - 1);
    if (setting->current_enum_idx == option_idx) return;

    setting->current_enum_idx = option_idx;
    update_enum_value_label(ctx, idx);

    if (notify && setting->on_change) {
        ui_setting_value_u val = {0};
        val.i32 = option_idx;
        setting->on_change(0, idx, &val);
    }
}

static void ensure_setting_tile_loaded(ui_page_settings_ctx_t *ctx, int idx)
{
    if (!ctx || !ctx->collection || !ctx->setting_tiles || !ctx->setting_tile_loaded) return;
    if (idx < 0 || idx >= ctx->collection->settings_count) return;
    if (ctx->setting_tile_loaded[idx]) return;

    create_setting_tile_widget(ctx, idx);
    ctx->setting_tile_loaded[idx] = true;
}

static void set_enum_label_highlight(ui_page_settings_ctx_t *ctx, int idx, bool highlighted)
{
    if (!ctx || !ctx->enum_name_labels || !ctx->collection) return;
    if (idx < 0 || idx >= ctx->collection->settings_count) return;

    lv_obj_t *label = ctx->enum_name_labels[idx];
    if (!label) return;

    if (highlighted) lv_obj_add_state(label, LV_STATE_USER_1);
    else lv_obj_clear_state(label, LV_STATE_USER_1);
}

/**
 * Derive encoder acceleration params from setting range
 */
void ui_setting_derive_accel_default(
    ui_setting_descriptor_t *setting,
    ui_encoder_accel_t *accel)
{
    if (!setting || !accel) return;

    float delta = setting->max_value - setting->min_value;
    if (delta < 0.0f) delta = -delta;
    if (delta < 1.0f) delta = 1.0f;
    float scale = log10f(delta + 1.0f);
    
    // Base step: constant per type
    accel->base_step = (setting->type == UI_SETTING_TYPE_FLOAT_BAR) ? 0.001f : 1.0f;
    
    // Gain: only mildly decreases with range so large ranges still ramp quickly.
    accel->gain_k = 0.035f / (1.0f + (0.20f * scale));
    
    // Tau: longer memory enables sustained fast rotation to accumulate better.
    accel->tau_ms = 380.0f;
    
    // Max accel: log-scaled cap to avoid runaway while still allowing larger jumps.
    accel->accel_max = 6.0f + (3.0f * scale);
}

static int clamp_index(int idx, int minv, int maxv)
{
    if (idx < minv) return minv;
    if (idx > maxv) return maxv;
    return idx;
}

static void create_placeholder_label(lv_obj_t *parent, const char *name, const char *value)
{
    lv_obj_t *name_label = lv_label_create(parent);
    if (name_label) {
        lv_obj_align(name_label, LV_ALIGN_CENTER, 0, -14);
        lv_label_set_text(name_label, name ? name : "Setting");
        lv_obj_add_style(name_label, &ui_style_light_mode, 0);
    }

    lv_obj_t *value_label = lv_label_create(parent);
    if (value_label) {
        lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 10);
        lv_label_set_text(value_label, value ? value : "N/A");
        lv_obj_add_style(value_label, &ui_style_light_mode, 0);
    }
}

static void create_bar_widget(ui_page_settings_ctx_t *ctx, int idx, lv_obj_t *parent)
{
    ui_setting_descriptor_t *setting = &ctx->collection->settings[idx];

    ui_encoder_accel_t accel = {0};
    ui_setting_derive_accel_default(setting, &accel);

    ui_setting_item_config_t cfg = {
        .name = setting->name,
        .value_type = (setting->type == UI_SETTING_TYPE_FLOAT_BAR) ? UI_SETTING_VALUE_FLOAT : UI_SETTING_VALUE_INT32,
        .commit_policy = UI_SETTING_COMMIT_LIVE,
        .accel = accel,
        .source = SOURCE_OLED_INDEV,
        .target = TARGET_LOG,
        .pool_type = DISPATCHER_POOL_STREAMING,
    };

    if (setting->type == UI_SETTING_TYPE_FLOAT_BAR) {
        cfg.initial_value.f32 = setting->current_value;
        cfg.min_value.f32 = setting->min_value;
        cfg.max_value.f32 = setting->max_value;
    } else {
        cfg.initial_value.i32 = (int32_t)lroundf(setting->current_value);
        cfg.min_value.i32 = (int32_t)lroundf(setting->min_value);
        cfg.max_value.i32 = (int32_t)lroundf(setting->max_value);
    }

    if (ui_setting_item_create_bar(&ctx->setting_items[idx], parent, &cfg) != ESP_OK) {
        ESP_LOGW(TAG, "failed to create int bar widget idx=%d", idx);
        create_placeholder_label(parent, setting->name, "Create failed");
        return;
    }

    ui_setting_item_set_ignore_next_tap(&ctx->setting_items[idx], true);
}

static void create_enum_widget(ui_page_settings_ctx_t *ctx, int idx, lv_obj_t *parent)
{
    ui_setting_descriptor_t *setting = &ctx->collection->settings[idx];

    lv_obj_t *name_label = lv_label_create(parent);
    if (name_label) {
        lv_obj_align(name_label, LV_ALIGN_CENTER, 0, -14);
        lv_label_set_text(name_label, setting->name ? setting->name : "Enum");
        lv_obj_add_style(name_label, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_style(name_label, &ui_style_dark_mode, LV_PART_MAIN | LV_STATE_USER_1);
        ctx->enum_name_labels[idx] = name_label;
    }

    lv_obj_t *value_label = lv_label_create(parent);
    if (!value_label) {
        ESP_LOGW(TAG, "failed to create enum value label idx=%d", idx);
        create_placeholder_label(parent, setting->name, "Create failed");
        return;
    }

    lv_obj_set_size(value_label, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_style(value_label, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_CLIP);

    int selected = clamp_index(setting->current_enum_idx, 0, setting->enum_count - 1);
    setting->current_enum_idx = selected;
    lv_label_set_text(value_label, get_enum_option_text(setting, selected));

    ctx->enum_value_labels[idx] = value_label;
}

static void create_setting_tile_widget(ui_page_settings_ctx_t *ctx, int idx)
{
    lv_obj_t *tile = ctx->setting_tiles[idx];
    if (!tile) return;

    ui_setting_descriptor_t *setting = &ctx->collection->settings[idx];
    switch (setting->type) {
        case UI_SETTING_TYPE_INT_BAR:
        case UI_SETTING_TYPE_FLOAT_BAR:
            create_bar_widget(ctx, idx, tile);
            break;
        case UI_SETTING_TYPE_ENUM_DROPDOWN:
            create_enum_widget(ctx, idx, tile);
            break;
        default:
            create_placeholder_label(tile, setting->name, "Unsupported");
            break;
    }
}

static void unload_setting_tile_widget(ui_page_settings_ctx_t *ctx, int idx)
{
    if (!ctx || !ctx->collection || !ctx->setting_tiles || !ctx->setting_tile_loaded) return;
    if (idx < 0 || idx >= ctx->collection->settings_count) return;
    if (!ctx->setting_tile_loaded[idx]) return;

    lv_obj_t *tile = ctx->setting_tiles[idx];
    if (!tile) return;

    ui_setting_item_destroy(&ctx->setting_items[idx]);
    lv_obj_clean(tile);

    ctx->enum_name_labels[idx] = NULL;
    ctx->enum_value_labels[idx] = NULL;
    memset(&ctx->setting_items[idx], 0, sizeof(ctx->setting_items[idx]));
    ctx->setting_tile_loaded[idx] = false;
}

static void set_active_setting(ui_page_settings_ctx_t *ctx, int idx)
{
    if (!ctx || !ctx->collection || !ctx->tileview) return;

    int prev_idx = clamp_index(ctx->active_setting_idx, 0, ctx->collection->settings_count - 1);
    set_enum_label_highlight(ctx, prev_idx, false);

    idx = clamp_index(idx, 0, ctx->collection->settings_count - 1);
    ensure_setting_tile_loaded(ctx, idx);
    ensure_setting_tile_loaded(ctx, idx - 1);
    ensure_setting_tile_loaded(ctx, idx + 1);

    for (int i = 0; i < ctx->collection->settings_count; ++i) {
        if (i >= idx - 1 && i <= idx + 1) continue;
        unload_setting_tile_widget(ctx, i);
    }

    ctx->active_setting_idx = idx;
    ctx->enum_editing = false;

    lv_tileview_set_tile_by_index(ctx->tileview, 0, idx, LV_ANIM_OFF);
}

static void on_encoder_input(int8_t dir)
{
    if (!s_page_ctx || !s_page_ctx->collection || s_page_ctx->collection->settings_count <= 0) return;
    if (!lvgl_port_lock(0)) return;

    if (dir == 0 && s_page_ctx->ignore_next_tap) {
        s_page_ctx->ignore_next_tap = false;
        lvgl_port_unlock();
        return;
    }

    int idx = clamp_index(s_page_ctx->active_setting_idx, 0, s_page_ctx->collection->settings_count - 1);
    ui_setting_descriptor_t *setting = &s_page_ctx->collection->settings[idx];
    ui_setting_item_t *item = &s_page_ctx->setting_items[idx];
    lv_obj_t *enum_value_label = s_page_ctx->enum_value_labels[idx];
    int64_t now_ms = esp_timer_get_time() / 1000;

    if (dir == 0) {
        if (setting->type == UI_SETTING_TYPE_ENUM_DROPDOWN && enum_value_label) {
            s_page_ctx->enum_editing = !s_page_ctx->enum_editing;
            set_enum_label_highlight(s_page_ctx, idx, s_page_ctx->enum_editing);
            set_enum_value_marquee(s_page_ctx, idx, s_page_ctx->enum_editing);
        } else if ((setting->type == UI_SETTING_TYPE_INT_BAR || setting->type == UI_SETTING_TYPE_FLOAT_BAR) &&
                   item->name_label) {
            ui_setting_item_handle_encoder(item, 0, now_ms);
        }

        lvgl_port_unlock();
        return;
    }

    if (setting->type == UI_SETTING_TYPE_INT_BAR && item->name_label && item->selected) {
        ui_setting_item_handle_encoder(item, dir, now_ms);

        setting->current_value = (float)item->value.i32;
        bool notify = (setting->on_change != NULL);
        ui_setting_value_u val = {0};
        val.i32 = item->value.i32;

        lvgl_port_unlock();

        if (notify) {
            setting->on_change(0, idx, &val);
        }

        return;
    }

    if (setting->type == UI_SETTING_TYPE_FLOAT_BAR && item->name_label && item->selected) {
        ui_setting_item_handle_encoder(item, dir, now_ms);

        setting->current_value = item->value.f32;
        bool notify = (setting->on_change != NULL);
        ui_setting_value_u val = {0};
        val.f32 = item->value.f32;

        lvgl_port_unlock();

        if (notify) {
            setting->on_change(0, idx, &val);
        }

        return;
    }

    if (setting->type == UI_SETTING_TYPE_ENUM_DROPDOWN && enum_value_label && s_page_ctx->enum_editing) {
        int current = clamp_index(setting->current_enum_idx, 0, setting->enum_count - 1);
        int next = clamp_index(current + dir, 0, setting->enum_count - 1);
        bool changed = (next != current);
        set_enum_value(s_page_ctx, idx, next, false);

        bool notify = changed && (setting->on_change != NULL);
        ui_setting_value_u val = {0};
        val.i32 = next;

        lvgl_port_unlock();

        if (notify) {
            setting->on_change(0, idx, &val);
        }

        return;
    }

    int next_idx = clamp_index(idx + dir, 0, s_page_ctx->collection->settings_count - 1);
    if (next_idx != idx) {
        set_active_setting(s_page_ctx, next_idx);
    }

    lvgl_port_unlock();
}

esp_err_t ui_page_settings_init(lv_obj_t *tile)
{
    if (!tile) return ESP_ERR_INVALID_ARG;
    if (!s_pending_collection || !s_pending_collection->settings || s_pending_collection->settings_count <= 0) {
        ESP_LOGW(TAG, "init failed: no settings collection");
        return ESP_ERR_INVALID_STATE;
    }

    s_page_ctx = calloc(1, sizeof(ui_page_settings_ctx_t));
    if (!s_page_ctx) return ESP_ERR_NO_MEM;

    lv_coord_t w = lv_obj_get_width(tile);
    lv_coord_t h = lv_obj_get_height(tile);
    s_page_ctx->container = tile;

    s_page_ctx->collection = s_pending_collection;
    s_page_ctx->active_setting_idx = 0;

    int count = s_page_ctx->collection->settings_count;
    s_page_ctx->setting_tiles = calloc((size_t)count, sizeof(lv_obj_t *));
    s_page_ctx->setting_tile_loaded = calloc((size_t)count, sizeof(bool));
    s_page_ctx->enum_value_labels = calloc((size_t)count, sizeof(lv_obj_t *));
    s_page_ctx->enum_name_labels = calloc((size_t)count, sizeof(lv_obj_t *));
    s_page_ctx->setting_items = calloc((size_t)count, sizeof(ui_setting_item_t));
    if (!s_page_ctx->setting_tiles || !s_page_ctx->setting_tile_loaded ||
        !s_page_ctx->enum_value_labels || !s_page_ctx->enum_name_labels || !s_page_ctx->setting_items) {
        free(s_page_ctx->setting_tiles);
        free(s_page_ctx->setting_tile_loaded);
        free(s_page_ctx->enum_value_labels);
        free(s_page_ctx->enum_name_labels);
        free(s_page_ctx->setting_items);
        free(s_page_ctx);
        s_page_ctx = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_page_ctx->tileview = lv_tileview_create(tile);
    if (!s_page_ctx->tileview) {
        free(s_page_ctx->setting_tiles);
        free(s_page_ctx->setting_tile_loaded);
        free(s_page_ctx->enum_value_labels);
        free(s_page_ctx->enum_name_labels);
        free(s_page_ctx->setting_items);
        free(s_page_ctx);
        s_page_ctx = NULL;
        return ESP_ERR_NO_MEM;
    }

    lv_obj_set_size(s_page_ctx->tileview, w, h - 16);
    lv_obj_set_align(s_page_ctx->tileview, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_style(s_page_ctx->tileview, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_page_ctx->tileview, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_bg_color(s_page_ctx->tileview, lv_color_black(), LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_border_width(s_page_ctx->tileview, 0, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_outline_width(s_page_ctx->tileview, 0, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_shadow_width(s_page_ctx->tileview, 0, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_pad_all(s_page_ctx->tileview, 0, LV_PART_MAIN | LV_STATE_ANY);

    for (int i = 0; i < count; i++) {
        lv_obj_t *setting_tile = lv_tileview_add_tile(s_page_ctx->tileview, 0, i, LV_DIR_VER);
        s_page_ctx->setting_tiles[i] = setting_tile;

        if (!setting_tile) continue;

        lv_obj_add_style(setting_tile, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(setting_tile, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_bg_color(setting_tile, lv_color_black(), LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_border_width(setting_tile, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_outline_width(setting_tile, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_shadow_width(setting_tile, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_set_style_pad_all(setting_tile, 0, LV_PART_MAIN | LV_STATE_ANY);
        lv_obj_clear_flag(setting_tile, LV_OBJ_FLAG_SCROLLABLE);
    }

    set_active_setting(s_page_ctx, 0);
    return ESP_OK;
}

void ui_page_settings_show(lv_obj_t *tile)
{
    (void)tile;
    if (!s_page_ctx || !s_page_ctx->collection) return;

    set_active_setting(s_page_ctx, s_page_ctx->active_setting_idx);
    s_page_ctx->ignore_next_tap = true;
    ui_input_set_widget_callback(on_encoder_input);
}

void ui_page_settings_hide(void)
{
    if (!s_page_ctx || !s_page_ctx->collection) return;

    for (int i = 0; i < s_page_ctx->collection->settings_count; i++) {
        ui_setting_item_t *item = &s_page_ctx->setting_items[i];
        if (item->name_label) {
            ui_setting_item_commit(item, UI_SETTING_COMMIT_REASON_PAGE_HIDE);
        }
    }

    s_page_ctx->enum_editing = false;
    s_page_ctx->ignore_next_tap = false;

    int active_idx = clamp_index(s_page_ctx->active_setting_idx, 0, s_page_ctx->collection->settings_count - 1);
    set_enum_label_highlight(s_page_ctx, active_idx, false);
    set_enum_value_marquee(s_page_ctx, active_idx, false);

    ui_input_set_widget_callback(NULL);
}

void ui_page_settings_deinit(void)
{
    if (!s_page_ctx) return;

    ui_input_set_widget_callback(NULL);

    if (s_page_ctx->collection && s_page_ctx->setting_items) {
        for (int i = 0; i < s_page_ctx->collection->settings_count; i++) {
            if (s_page_ctx->setting_items[i].name_label) {
                ui_setting_item_destroy(&s_page_ctx->setting_items[i]);
            }
        }
    }

    if (s_page_ctx->tileview) {
        lv_obj_del(s_page_ctx->tileview);
        s_page_ctx->tileview = NULL;
    }

    s_page_ctx->container = NULL;

    free(s_page_ctx->setting_tiles);
    free(s_page_ctx->setting_tile_loaded);
    free(s_page_ctx->enum_value_labels);
    free(s_page_ctx->enum_name_labels);
    free(s_page_ctx->setting_items);
    free(s_page_ctx);
    s_page_ctx = NULL;
}

void ui_page_settings_set_collection(const ui_setting_collection_t *collection)
{
    s_pending_collection = collection;
    if (s_page_ctx) s_page_ctx->collection = collection;
}
