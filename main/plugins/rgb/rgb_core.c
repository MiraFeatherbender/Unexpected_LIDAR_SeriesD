#include "rgb_core.h"
#include "dispatcher.h"
#include "dispatcher_module.h"
#include "rest_context.h"
#include "rgb_commands.h"
#include "rgb_state_json.h"
#include "rgb_anim_dynamic.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define RGB_CORE_CMD_QUEUE_LEN 8
#define RGB_CORE_TASK_STACK_SIZE 4096
#define RGB_CORE_TASK_PRIORITY 5

static void rgb_core_process_msg(const dispatcher_msg_t *msg);

static dispatcher_module_t rgb_core_mod = {
    .name = "rgb_core_task",
    .target = TARGET_RGB,
    .queue_len = RGB_CORE_CMD_QUEUE_LEN,
    .stack_size = RGB_CORE_TASK_STACK_SIZE,
    .task_prio = RGB_CORE_TASK_PRIORITY,
    .process_msg = rgb_core_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL,
    .next_step = 0,
};

typedef enum {
    RGB_PLUGIN_TYPE_HSV = 0,
    RGB_PLUGIN_TYPE_RGB = 1,
} rgb_plugin_type_t;

typedef struct {
    rgb_plugin_type_t type;
    union {
        const hsv_anim_t *hsv;
        const rgb_anim_t *rgb;
    } plugin;
} rgb_plugin_entry_t;

static rgb_plugin_entry_t s_rgb_plugins[RGB_PLUGIN_MAX] = {0};

static const rgb_plugin_entry_t *s_active_anim = NULL;
static hsv_color_t s_current_hsv = {0, 0, 0};
static uint8_t s_current_brightness = 255;
static uint8_t s_phase_fallback = 0;
static uint8_t *s_phase_ptr = &s_phase_fallback;

static uint8_t s_last_plugin_id = RGB_PLUGIN_MAX;
static hsv_color_t s_last_hsv = {0, 0, 0};
static uint8_t s_last_brightness = 255;
static bool s_has_last = false;

enum { SRC_V = 0, SRC_P = 1, SRC_Q = 2, SRC_T = 3 };

static const uint8_t s_rgb_src[6][3] = {
    { SRC_V, SRC_T, SRC_P },
    { SRC_Q, SRC_V, SRC_P },
    { SRC_P, SRC_V, SRC_T },
    { SRC_P, SRC_Q, SRC_V },
    { SRC_T, SRC_P, SRC_V },
    { SRC_V, SRC_P, SRC_Q },
};

void rgb_core_init(void)
{
    if (dispatcher_module_start(&rgb_core_mod) != pdTRUE) {
        ESP_LOGE("rgb_core", "Failed to start dispatcher module for rgb_core");
        return;
    }
}

void rgb_core_set_phase_ptr(uint8_t *phase_u8)
{
    s_phase_ptr = phase_u8 ? phase_u8 : &s_phase_fallback;
}

static void hsv8_to_rgb888(uint8_t h, uint8_t s, uint8_t v,
                           uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    uint16_t h6 = (uint16_t)h * 6;
    uint8_t region = h6 >> 8;
    uint8_t remainder = h6 & 0xFF;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    const uint8_t *src = s_rgb_src[region];
    uint8_t vals[4] = { v, p, q, t };

    *r = vals[src[0]];
    *g = vals[src[1]];
    *b = vals[src[2]];
}

void rgb_core_register_hsv_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin)
{
    if (id < RGB_PLUGIN_MAX) {
        s_rgb_plugins[id].type = RGB_PLUGIN_TYPE_HSV;
        s_rgb_plugins[id].plugin.hsv = plugin;
    }
}

void rgb_core_register_rgb_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin)
{
    if (id < RGB_PLUGIN_MAX) {
        s_rgb_plugins[id].type = RGB_PLUGIN_TYPE_RGB;
        s_rgb_plugins[id].plugin.rgb = plugin;
    }
}

void rgb_core_register_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin)
{
    rgb_core_register_hsv_plugin(id, plugin);
}

void io_rgb_register_hsv_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin)
{
    rgb_core_register_hsv_plugin(id, plugin);
}

void io_rgb_register_rgb_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin)
{
    rgb_core_register_rgb_plugin(id, plugin);
}

void io_rgb_register_plugin(rgb_plugin_id_t id, const hsv_anim_t *plugin)
{
    rgb_core_register_plugin(id, plugin);
}

void rgb_core_apply_command(uint8_t plugin_id, uint8_t h, uint8_t s, uint8_t v, uint8_t brightness)
{
    hsv_color_t new_hsv = {h, s, v};
    uint8_t new_brightness = brightness;

    bool plugin_changed = !s_has_last || (plugin_id != s_last_plugin_id);
    bool params_changed =
        !s_has_last ||
        (new_hsv.h != s_last_hsv.h) ||
        (new_hsv.s != s_last_hsv.s) ||
        (new_hsv.v != s_last_hsv.v) ||
        (new_brightness != s_last_brightness);

    s_active_anim = &s_rgb_plugins[plugin_id];

    if (s_active_anim) {
        if (s_active_anim->type == RGB_PLUGIN_TYPE_HSV && s_active_anim->plugin.hsv) {
            if (plugin_changed && s_active_anim->plugin.hsv->begin)
                s_active_anim->plugin.hsv->begin(s_phase_ptr);

            if (params_changed) {
                if (s_active_anim->plugin.hsv->set_color)
                    s_active_anim->plugin.hsv->set_color(new_hsv);

                if (s_active_anim->plugin.hsv->set_brightness)
                    s_active_anim->plugin.hsv->set_brightness(new_brightness);
            }
        } else if (s_active_anim->type == RGB_PLUGIN_TYPE_RGB && s_active_anim->plugin.rgb) {
            if (plugin_changed) {
                rgb_anim_dynamic_select_plugin(plugin_id);
                if (s_active_anim->plugin.rgb->begin) {
                    s_active_anim->plugin.rgb->begin(s_phase_ptr);
                }
            }

            if (params_changed) {
                if (s_active_anim->plugin.rgb->set_brightness)
                    s_active_anim->plugin.rgb->set_brightness(new_brightness);
            }
        }
    }

    s_current_hsv = new_hsv;
    s_current_brightness = new_brightness;
    s_last_plugin_id = plugin_id;
    s_last_hsv = new_hsv;
    s_last_brightness = new_brightness;
    s_has_last = true;
}

bool rgb_core_step_rgb(rgb_color_t *out_rgb)
{
    if (!out_rgb || !s_active_anim) {
        return false;
    }

    if (s_active_anim->type == RGB_PLUGIN_TYPE_HSV && s_active_anim->plugin.hsv) {
        hsv_color_t out_hsv = s_current_hsv;
        if (s_active_anim->plugin.hsv->step)
            s_active_anim->plugin.hsv->step(&out_hsv);

        hsv8_to_rgb888(out_hsv.h, out_hsv.s, out_hsv.v, &out_rgb->r, &out_rgb->g, &out_rgb->b);
        return true;
    }

    if (s_active_anim->type == RGB_PLUGIN_TYPE_RGB && s_active_anim->plugin.rgb) {
        out_rgb->r = 0;
        out_rgb->g = 0;
        out_rgb->b = 0;
        if (s_active_anim->plugin.rgb->step)
            s_active_anim->plugin.rgb->step(out_rgb);
        return true;
    }

    return false;
}

void rgb_core_get_snapshot(rgb_core_snapshot_t *snapshot)
{
    if (!snapshot) {
        return;
    }

    snapshot->plugin_id = s_last_plugin_id;
    snapshot->hsv = s_current_hsv;
    snapshot->brightness = s_current_brightness;
}

static void rgb_core_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg) {
        return;
    }

    if (msg->source == SOURCE_REST && msg->context) {
        rest_json_request_t *req = (rest_json_request_t *)msg->context;
        rgb_core_snapshot_t snapshot = {0};
        rgb_core_get_snapshot(&snapshot);
        rgb_state_json_fill(req, snapshot.plugin_id, snapshot.hsv, snapshot.brightness);
        xSemaphoreGive(req->sem);
        return;
    }

    if (msg->message_len >= 6) {
        uint8_t cmd = msg->data[5];
        switch (cmd) {
            case RGB_CMD_RELOAD:
                rgb_anim_dynamic_reload();
                break;
            default:
                break;
        }
    }
}
