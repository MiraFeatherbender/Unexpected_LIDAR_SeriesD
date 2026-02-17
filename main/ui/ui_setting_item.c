#include "ui_setting_item.h"

#include <stdio.h>
#include <string.h>

#include "ui_dispatch_bridge.h"
#include "ui_styles.h"

#define UI_SETTING_COMMIT_KEY(policy, reason) ((((uint16_t)(policy)) << 8) | ((uint16_t)(reason)))
#define UI_SETTING_COMMIT_CASE(policy, reason) case UI_SETTING_COMMIT_KEY((policy), (reason))

#define UI_SETTING_LABEL_NAME_Y   (-16)
#define UI_SETTING_LABEL_VALUE_Y  (16)
#define UI_SETTING_BAR_NAME_Y     (-12)
#define UI_SETTING_BAR_Y          (12)
#define UI_SETTING_BAR_WIDTH  (104)
#define UI_SETTING_BAR_HEIGHT (12)

static void ui_setting_item_bar_draw_value_cb(lv_event_t *e)
{
    ui_setting_item_t *item = (ui_setting_item_t *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (!item || !obj) return;

    int32_t minv = item->min_value.i32;
    int32_t maxv = item->max_value.i32;
    int32_t value = lv_bar_get_value(obj);
    int32_t range = maxv - minv;
    if (range <= 0) return;

    int32_t rel = value - minv;
    if (rel < 0) rel = 0;
    if (rel > range) rel = range;

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%d", (int)value);

    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space, label_dsc.line_space, LV_COORD_MAX,
                     label_dsc.flag);

    lv_area_t txt_area;
    txt_area.x1 = 0;
    txt_area.x2 = txt_size.x - 1;
    txt_area.y1 = 0;
    txt_area.y2 = txt_size.y - 1;

    lv_area_t indic_area;
    lv_obj_get_coords(obj, &indic_area);
    int32_t indic_w = (lv_area_get_width(&indic_area) * rel) / range;
    lv_area_set_width(&indic_area, indic_w);

    if (indic_w > (txt_size.x + 12)) {
        lv_area_align(&indic_area, &txt_area, LV_ALIGN_RIGHT_MID, -6, 0);
        label_dsc.color = lv_color_black();
    } else {
        lv_area_align(&indic_area, &txt_area, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
        label_dsc.color = lv_color_white();
    }

    label_dsc.text = buf;
    label_dsc.text_local = true;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_dsc, &txt_area);
}

static int32_t ui_setting_item_clamp_i32(int32_t value, int32_t minv, int32_t maxv)
{
    if (value < minv) return minv;
    if (value > maxv) return maxv;
    return value;
}

static void ui_setting_item_reset_accel_runtime(ui_setting_item_t *item)
{
    if (!item) return;
    item->accel.accum = 0.0f;
    item->accel.residual = 0.0f;
    item->accel.last_ms = 0;
}

static void ui_setting_item_sync_selected(ui_setting_item_t *item)
{
    if (!item || !item->selected_subject_initialized) return;
    lv_subject_set_int(&item->selected_subject, item->selected ? 1 : 0);
}

static void ui_setting_item_sync_value(ui_setting_item_t *item)
{
    if (!item) return;

    switch (item->value_type) {
        case UI_SETTING_VALUE_INT32:
            if (item->value_subject_initialized) {
                lv_subject_set_int(&item->value_subject, item->value.i32);
            }
            if (item->bar) lv_bar_set_value(item->bar, item->value.i32, LV_ANIM_OFF);
            break;
        default:
            break;
    }
}

static esp_err_t ui_setting_item_send(ui_setting_item_t *item)
{
    if (!item) return ESP_ERR_INVALID_ARG;

    char msg[96];
    switch (item->value_type) {
        case UI_SETTING_VALUE_INT32:
            snprintf(msg, sizeof(msg), "%s=%ld", item->name ? item->name : "value", (long)item->value.i32);
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = item->target;

    dispatcher_pool_send_params_t params = {
        .type = item->pool_type,
        .source = item->source,
        .targets = targets,
        .data = (const uint8_t *)msg,
        .data_len = strlen(msg),
        .context = NULL,
    };

    return ui_dispatch_bridge_send_params(&params);
}

void ui_setting_item_commit(ui_setting_item_t *item, ui_setting_commit_reason_t reason)
{
    if (!item || !item->dirty) return;

    uint16_t key = UI_SETTING_COMMIT_KEY(item->commit_policy, reason);
    switch (key) {
        UI_SETTING_COMMIT_CASE(UI_SETTING_COMMIT_LIVE, UI_SETTING_COMMIT_REASON_ROTATE):
        UI_SETTING_COMMIT_CASE(UI_SETTING_COMMIT_ON_DESELECT, UI_SETTING_COMMIT_REASON_DESELECT):
        UI_SETTING_COMMIT_CASE(UI_SETTING_COMMIT_DEFERRED, UI_SETTING_COMMIT_REASON_PAGE_HIDE):
        UI_SETTING_COMMIT_CASE(UI_SETTING_COMMIT_LIVE, UI_SETTING_COMMIT_REASON_EXPLICIT):
        UI_SETTING_COMMIT_CASE(UI_SETTING_COMMIT_ON_DESELECT, UI_SETTING_COMMIT_REASON_EXPLICIT):
        UI_SETTING_COMMIT_CASE(UI_SETTING_COMMIT_DEFERRED, UI_SETTING_COMMIT_REASON_EXPLICIT):
            if (ui_setting_item_send(item) == ESP_OK) item->dirty = false;
            break;
        default:
            break;
    }
}

esp_err_t ui_setting_item_create_labels(ui_setting_item_t *item, lv_obj_t *parent, const ui_setting_item_config_t *cfg)
{
    if (!item || !parent || !cfg) return ESP_ERR_INVALID_ARG;

    memset(item, 0, sizeof(*item));
    item->name = cfg->name;
    item->value_type = cfg->value_type;
    item->value = cfg->initial_value;
    item->min_value = cfg->min_value;
    item->max_value = cfg->max_value;
    item->commit_policy = cfg->commit_policy;
    item->accel = cfg->accel;
    ui_setting_item_reset_accel_runtime(item);
    item->source = cfg->source;
    item->target = cfg->target;
    item->pool_type = cfg->pool_type;

    item->name_label = lv_label_create(parent);
    if (!item->name_label) return ESP_ERR_NO_MEM;

    lv_obj_align(item->name_label, LV_ALIGN_CENTER, 0, UI_SETTING_LABEL_NAME_Y);
    lv_label_set_text(item->name_label, cfg->name ? cfg->name : "Value");
    lv_obj_add_style(item->name_label, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(item->name_label, &ui_style_dark_mode, LV_PART_MAIN | LV_STATE_USER_1);

    lv_subject_init_int(&item->selected_subject, 0);
    item->selected_subject_initialized = true;
    lv_obj_bind_state_if_eq(item->name_label, &item->selected_subject, LV_STATE_USER_1, 1);

    item->value_label = lv_label_create(parent);
    if (!item->value_label) return ESP_ERR_NO_MEM;

    lv_obj_align(item->value_label, LV_ALIGN_CENTER, 0, UI_SETTING_LABEL_VALUE_Y);
    lv_obj_add_style(item->value_label, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);

    switch (item->value_type) {
        case UI_SETTING_VALUE_INT32:
            lv_subject_init_int(&item->value_subject, item->value.i32);
            item->value_subject_initialized = true;
            lv_label_bind_text(item->value_label, &item->value_subject, "%d");
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    ui_encoder_accel_reset(&item->accel);
    ui_setting_item_sync_selected(item);
    ui_setting_item_sync_value(item);
    return ESP_OK;
}

esp_err_t ui_setting_item_create_bar(ui_setting_item_t *item, lv_obj_t *parent, const ui_setting_item_config_t *cfg)
{
    if (!item || !parent || !cfg) return ESP_ERR_INVALID_ARG;

    memset(item, 0, sizeof(*item));
    item->name = cfg->name;
    item->value_type = cfg->value_type;
    item->value = cfg->initial_value;
    item->min_value = cfg->min_value;
    item->max_value = cfg->max_value;
    item->commit_policy = cfg->commit_policy;
    item->accel = cfg->accel;
    ui_setting_item_reset_accel_runtime(item);
    item->source = cfg->source;
    item->target = cfg->target;
    item->pool_type = cfg->pool_type;

    item->name_label = lv_label_create(parent);
    if (!item->name_label) return ESP_ERR_NO_MEM;

    lv_obj_align(item->name_label, LV_ALIGN_CENTER, 0, UI_SETTING_BAR_NAME_Y);
    lv_label_set_text(item->name_label, cfg->name ? cfg->name : "Value");
    lv_obj_add_style(item->name_label, &ui_style_light_mode, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(item->name_label, &ui_style_dark_mode, LV_PART_MAIN | LV_STATE_USER_1);

    lv_subject_init_int(&item->selected_subject, 0);
    item->selected_subject_initialized = true;
    lv_obj_bind_state_if_eq(item->name_label, &item->selected_subject, LV_STATE_USER_1, 1);

    item->bar = lv_bar_create(parent);
    if (!item->bar) return ESP_ERR_NO_MEM;

    // lv_obj_remove_style_all(item->bar);
    lv_obj_set_size(item->bar, UI_SETTING_BAR_WIDTH, UI_SETTING_BAR_HEIGHT);
    lv_obj_align(item->bar, LV_ALIGN_CENTER, 0, UI_SETTING_BAR_Y);

    lv_obj_set_style_bg_opa(item->bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_bg_color(item->bar, lv_color_white(), LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_border_opa(item->bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_border_color(item->bar, lv_color_white(), LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_border_width(item->bar, 1, LV_PART_MAIN | LV_STATE_ANY);
    lv_obj_set_style_radius(item->bar, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_ANY);

    lv_obj_set_style_bg_opa(item->bar, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_ANY);
    lv_obj_set_style_bg_color(item->bar, lv_color_white(), LV_PART_INDICATOR | LV_STATE_ANY);
    lv_obj_set_style_radius(item->bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_ANY);

    switch (item->value_type) {
        case UI_SETTING_VALUE_INT32:
            lv_bar_set_range(item->bar, item->min_value.i32, item->max_value.i32);
            lv_obj_add_event_cb(item->bar, ui_setting_item_bar_draw_value_cb, LV_EVENT_DRAW_MAIN_END, item);
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    ui_encoder_accel_reset(&item->accel);
    ui_setting_item_sync_selected(item);
    ui_setting_item_sync_value(item);
    return ESP_OK;
}

void ui_setting_item_destroy(ui_setting_item_t *item)
{
    if (!item) return;

    if (item->value_subject_initialized) {
        lv_subject_deinit(&item->value_subject);
        item->value_subject_initialized = false;
    }

    if (item->selected_subject_initialized) {
        lv_subject_deinit(&item->selected_subject);
        item->selected_subject_initialized = false;
    }

    item->name_label = NULL;
    item->bar = NULL;
    item->value_label = NULL;
}

void ui_setting_item_set_ignore_next_tap(ui_setting_item_t *item, bool ignore)
{
    if (!item) return;
    item->ignore_next_tap = ignore;
}

void ui_setting_item_handle_encoder(ui_setting_item_t *item, int8_t dir, int64_t now_ms)
{
    if (!item || !item->name_label) return;

    switch (dir) {
        case 0:
            if (item->ignore_next_tap) {
                item->ignore_next_tap = false;
                return;
            }

            item->selected = !item->selected;
            ui_setting_item_sync_selected(item);

            switch (item->selected) {
                case false:
                    ui_encoder_accel_reset(&item->accel);
                    ui_setting_item_commit(item, UI_SETTING_COMMIT_REASON_DESELECT);
                    break;
                case true:
                default:
                    break;
            }
            break;

        default:
            if (!item->selected) return;

            switch (item->value_type) {
                case UI_SETTING_VALUE_INT32: {
                    int32_t prev = item->value.i32;
                    int32_t delta = ui_encoder_accel_apply(&item->accel, dir, now_ms);
                    int32_t next = prev + delta;
                    next = ui_setting_item_clamp_i32(next, item->min_value.i32, item->max_value.i32);
                    item->value.i32 = next;
                    if (next != prev) {
                        item->dirty = true;
                        ui_setting_item_sync_value(item);
                        ui_setting_item_commit(item, UI_SETTING_COMMIT_REASON_ROTATE);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
    }
}
