#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "dispatcher.h"
#include "dispatcher/dispatcher_pool.h"
#include "ui_encoder_accel.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SETTING_VALUE_INT32 = 0,
    UI_SETTING_VALUE_FLOAT,
    UI_SETTING_VALUE_BOOL,
    UI_SETTING_VALUE_UINT32,
} ui_setting_value_type_t;

typedef enum {
    UI_SETTING_WIDGET_LABELS = 0,
    UI_SETTING_WIDGET_BAR,
} ui_setting_widget_kind_t;

typedef union {
    int32_t i32;
    float f32;
    bool b;
    uint32_t u32;
} ui_setting_value_u;

typedef enum {
    UI_SETTING_COMMIT_LIVE = 0,
    UI_SETTING_COMMIT_ON_DESELECT,
    UI_SETTING_COMMIT_DEFERRED,
} ui_setting_commit_policy_t;

typedef enum {
    UI_SETTING_COMMIT_REASON_ROTATE = 0,
    UI_SETTING_COMMIT_REASON_DESELECT,
    UI_SETTING_COMMIT_REASON_PAGE_HIDE,
    UI_SETTING_COMMIT_REASON_EXPLICIT,
} ui_setting_commit_reason_t;

typedef struct {
    const char *name;
    ui_setting_value_type_t value_type;
    ui_setting_value_u initial_value;
    ui_setting_value_u min_value;
    ui_setting_value_u max_value;
    ui_setting_commit_policy_t commit_policy;
    ui_encoder_accel_t accel;
    dispatch_source_t source;
    dispatch_target_t target;
    dispatcher_pool_type_t pool_type;
} ui_setting_item_config_t;

typedef struct {
    const char *name;
    ui_setting_value_type_t value_type;
    ui_setting_value_u value;
    ui_setting_value_u min_value;
    ui_setting_value_u max_value;
    ui_setting_commit_policy_t commit_policy;
    ui_encoder_accel_t accel;
    dispatch_source_t source;
    dispatch_target_t target;
    dispatcher_pool_type_t pool_type;
    int32_t bar_scale;

    bool selected;
    bool dirty;
    bool ignore_next_tap;

    lv_obj_t *name_label;
    lv_obj_t *bar;
    lv_obj_t *value_label;
    lv_subject_t selected_subject;
    bool selected_subject_initialized;
    lv_subject_t value_subject;
    bool value_subject_initialized;
} ui_setting_item_t;

esp_err_t ui_setting_item_create_labels(ui_setting_item_t *item, lv_obj_t *parent, const ui_setting_item_config_t *cfg);
esp_err_t ui_setting_item_create_bar(ui_setting_item_t *item, lv_obj_t *parent, const ui_setting_item_config_t *cfg);
void ui_setting_item_destroy(ui_setting_item_t *item);

void ui_setting_item_set_ignore_next_tap(ui_setting_item_t *item, bool ignore);
void ui_setting_item_handle_encoder(ui_setting_item_t *item, int8_t dir, int64_t now_ms);
void ui_setting_item_commit(ui_setting_item_t *item, ui_setting_commit_reason_t reason);

#ifdef __cplusplus
}
#endif
