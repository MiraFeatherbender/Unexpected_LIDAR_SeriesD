#include "io_rgb_led.h"

#include "dispatcher.h"
#include "dispatcher_module.h"
#include "io_rgb_led_fnl_contract.h"
#include "rgb_core.h"
#include "rgb_anim.h"
#include "rgb_anim_dynamic.h"

#include "esp_check.h"
#include "esp_log.h"
#include "led_strip.h"
#include "LED_disk_points_241_outwards.h"
#include "FastNoiseLite.h"
#include "math.h"
#include "stdint.h"
#include <string.h>

#if CONFIG_FREERTOS_UNICORE
#define LED_STRIP_TASK_CORE_ID 0
#else
#define LED_STRIP_TASK_CORE_ID 1
#endif

#define LED_STRIP_GPIO                1
#define LED_STRIP_LENGTH              DISK_POINTS_COUNT
#define LED_STRIP_TASK_STACK_WORDS    3072
#define LED_STRIP_TASK_PRIORITY       5
#define LED_STRIP_STEP_DELAY_MS       33
#define LED_STRIP_CMD_QUEUE_LEN       8

static const char *TAG = "io_rgb_led";
static led_strip_handle_t s_led_strip = NULL;

static rgb_core_sample_in_t s_led_anim_state[LED_STRIP_LENGTH];
static uint8_t s_led_phase_u8[LED_STRIP_LENGTH];

// Temporary test vector: heartbeat plugin with existing baseline HSVB values
static const rgb_plugin_id_t s_strip_test_plugin = RGB_PLUGIN_FIRE;
static const hsv_color_t s_strip_test_hsv = { .h = 88, .s = 255, .v = 220 };
static const uint8_t s_strip_test_brightness = 128;

static fnl_state s_noise_active;
static fnl_state s_noise_pending;
static bool s_noise_pending_valid = false;
static uint32_t s_last_applied_version = 0;
static uint32_t s_pending_version = 0;

static void io_rgb_led_process_msg(const dispatcher_msg_t *msg);
static void io_rgb_led_step_frame(void);

static void io_rgb_led_reset_anim_state(void)
{
    for (uint32_t index = 0; index < LED_STRIP_LENGTH; ++index) {
        s_led_phase_u8[index] = 0;
        s_led_anim_state[index].plugin_id = s_strip_test_plugin;
        s_led_anim_state[index].brightness = s_strip_test_brightness;
        s_led_anim_state[index].base_hsv = s_strip_test_hsv;
        s_led_anim_state[index].phase_u8 = &s_led_phase_u8[index];
        s_led_anim_state[index].noise_u8 = 127;
    }
}

static dispatcher_module_t io_rgb_led_mod = {
    .name = "io_rgb_led",
    .target = TARGET_RGB_STRIP,
    .queue_len = LED_STRIP_CMD_QUEUE_LEN,
    .stack_size = LED_STRIP_TASK_STACK_WORDS,
    .task_prio = LED_STRIP_TASK_PRIORITY,
    .process_msg = io_rgb_led_process_msg,
    .step_frame = io_rgb_led_step_frame,
    .step_ms = LED_STRIP_STEP_DELAY_MS,
    .queue = NULL,
    .next_step = 0,
    .last_queue_warn = 0,
    .pin_to_core = true,
    .core_id = LED_STRIP_TASK_CORE_ID,
};

static inline uint8_t noise_f32_to_u8(float noise_f32)
{
    if (noise_f32 < -1.0f) noise_f32 = -1.0f;
    if (noise_f32 >  1.0f) noise_f32 =  1.0f;
    return (uint8_t)lroundf((noise_f32 + 1.0f) * 127.5f);
}

static void io_rgb_led_apply_pending_state_if_any(void)
{
    if (!s_noise_pending_valid) {
        return;
    }

    s_noise_active = s_noise_pending;
    s_noise_pending_valid = false;
    if (s_pending_version != 0) {
        s_last_applied_version = s_pending_version;
    }
}

static void io_rgb_led_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg || !msg->context) {
        return;
    }

    rgb_led_fnl_ctx_t *ctx = (rgb_led_fnl_ctx_t *)msg->context;
    ctx->status = RGB_LED_FNL_OK;

    if (!ctx->state || ctx->state_size != sizeof(fnl_state)) {
        ctx->status = ctx->state ? RGB_LED_FNL_ERR_SIZE : RGB_LED_FNL_ERR_ARG;
        if (ctx->sem) {
            xSemaphoreGive(ctx->sem);
        }
        return;
    }

    switch (ctx->op) {
        case RGB_LED_FNL_OP_GET_SNAPSHOT:
            memcpy(ctx->state, &s_noise_active, sizeof(fnl_state));
            break;

        case RGB_LED_FNL_OP_SET_SNAPSHOT:
            if (ctx->version != 0 && !rgb_led_fnl_version_is_newer(ctx->version, s_last_applied_version)) {
                ctx->status = RGB_LED_FNL_ERR_VERSION_OLD;
                break;
            }
            memcpy(&s_noise_pending, ctx->state, sizeof(fnl_state));
            s_noise_pending_valid = true;
            s_pending_version = ctx->version;
            break;

        default:
            ctx->status = RGB_LED_FNL_ERR_ARG;
            break;
    }

    if (ctx->sem) {
        xSemaphoreGive(ctx->sem);
    }
}

static void io_rgb_led_step_frame(void)
{
    io_rgb_led_apply_pending_state_if_any();

    const bool plugin_is_hsv = rgb_core_plugin_is_hsv(s_strip_test_plugin);
    const bool plugin_is_rgb = rgb_core_plugin_is_rgb(s_strip_test_plugin);

    if (!plugin_is_hsv && !plugin_is_rgb) {
        return;
    }

    for (uint32_t index = 0; index < LED_STRIP_LENGTH; ++index) {
        float noise_f32 = fnlGetNoise2D(&s_noise_active, disk_points[index].x, disk_points[index].y);
        uint8_t noise_u8 = noise_f32_to_u8(noise_f32);

        rgb_core_sample_in_t *sample = &s_led_anim_state[index];
        sample->plugin_id = s_strip_test_plugin;
        sample->brightness = s_strip_test_brightness;
        sample->base_hsv = s_strip_test_hsv;
        sample->noise_u8 = noise_u8;

        rgb_color_t out_rgb = {0, 0, 0};
        if (!rgb_core_sample(sample, &out_rgb)) {
            return;
        }

        ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, index, out_rgb.r, out_rgb.g, out_rgb.b));
    }

    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}

esp_err_t io_rgb_led_start(void)
{
    if (s_led_strip != NULL) {
        return ESP_OK;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LENGTH,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = true,
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED strip on GPIO %d: %s", LED_STRIP_GPIO, esp_err_to_name(err));
        return err;
    }

    ESP_RETURN_ON_ERROR(led_strip_clear(s_led_strip), TAG, "Failed to clear LED strip");

    s_noise_active = fnlCreateState();
    s_noise_active.frequency = 0.2f;
    s_noise_pending = s_noise_active;
    s_noise_pending_valid = false;
    s_last_applied_version = 0;
    s_pending_version = 0;
    io_rgb_led_reset_anim_state();

    if (rgb_core_plugin_is_rgb(s_strip_test_plugin)) {
        rgb_anim_dynamic_request_preload(s_strip_test_plugin);
    }

    if (dispatcher_module_start(&io_rgb_led_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for io_rgb_led");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED strip initialized on GPIO %d (%d LEDs)", LED_STRIP_GPIO, LED_STRIP_LENGTH);
    return ESP_OK;
}
