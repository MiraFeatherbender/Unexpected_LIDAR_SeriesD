#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "io_gpio.h"
#include "soc/gpio_reg.h"
#include "driver/gpio.h"
#include "driver/gpio_filter.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "io_gpio";

static QueueHandle_t gpio_event_queue = NULL;
static gpio_glitch_filter_handle_t button_glitch_filter = NULL;
/* Per-pin glitch filter handles for the line sensor (one handle per input pin). */
static gpio_glitch_filter_handle_t line_sensor_glitch_filters[8] = { NULL };

void io_gpio_init(void) {
    gpio_event_queue = xQueueCreate(IO_GPIO_EVENT_QUEUE_LEN, sizeof(gpio_isr_msg_t));   
    gpio_install_isr_service(0); 
    setup_button();
    setup_line_sensor();

    // Start GPIO event task
    xTaskCreate(io_gpio_event_task, "io_gpio_event_task", 4096, NULL, 9, NULL);

}

// Helper: pack pin states from all_gpio for a set
IRAM_ATTR static inline uint8_t pack_gpio_set(const gpio_num_t *pins, size_t pin_count, uint64_t all_gpio) {
    uint8_t state = 0;
    for (size_t i = 0; i < pin_count; ++i) {
        state <<= 1;
        state |= (all_gpio >> pins[i]) & 0x01;
    }
    return state;
}

IRAM_ATTR static inline uint64_t read_all_gpio(void) {
    uint32_t all_lower_gpio = REG_READ(GPIO_IN_REG);
    uint32_t all_upper_gpio = REG_READ(GPIO_IN1_REG);
    return ((uint64_t)all_upper_gpio << 32) | all_lower_gpio;
}

// Debounce packed line sensor state
// Tunable parameters — adjust these for tradeoff between latency and
// robustness during testing.
#define LINE_DEBOUNCE_SAMPLES      32
#define LINE_DEBOUNCE_SAMPLE_US    50
#define LINE_DEBOUNCE_STABLE_COUNT 2

// NOTE: This function is placed in IRAM and safe to call from ISR context,
// but it is blocking: it performs busy-wait delays. Worst-case duration is
// approximately LINE_DEBOUNCE_SAMPLES * LINE_DEBOUNCE_SAMPLE_US (us).
IRAM_ATTR static bool debounce_line_sensor(const gpio_num_t *pins, size_t pin_count, uint8_t *out_state) {
    if (!out_state) return false;
    uint32_t history = 0;

    /* Compute mask for the lower N bytes we care about. For stable_count >= 4
       use full 32-bit mask to avoid undefined shifts. */
    const uint32_t stable_mask = (LINE_DEBOUNCE_STABLE_COUNT >= 4) ? 0xFFFFFFFFu : ((1u << (8 * LINE_DEBOUNCE_STABLE_COUNT)) - 1u);

    uint32_t pattern_mult;
    if (LINE_DEBOUNCE_STABLE_COUNT == 1) pattern_mult = 0x01u;
    else if (LINE_DEBOUNCE_STABLE_COUNT == 2) pattern_mult = 0x0101u;
    else if (LINE_DEBOUNCE_STABLE_COUNT == 3) pattern_mult = 0x010101u;
    else pattern_mult = 0x01010101u;

    for (int i = 0; i < LINE_DEBOUNCE_SAMPLES; ++i) {
        esp_rom_delay_us(LINE_DEBOUNCE_SAMPLE_US);
        uint8_t cur = pack_gpio_set(pins, pin_count, read_all_gpio());
        history = (history << 8) | cur;

        if (i < (LINE_DEBOUNCE_STABLE_COUNT - 1)) {
            continue; // need more samples before checking
        }

        uint32_t pattern = ((uint32_t)cur) * pattern_mult;
        if ((history & stable_mask) != pattern) {
            continue; // not stable yet
        }

        *out_state = cur;
        return true;
    }

    return false;
}


// Generic GPIO ISR registration function
void register_gpio_isr(
    const gpio_num_t *pins, size_t pin_count,
    gpio_isr_t isr_func, void *isr_ctx, gpio_config_t *cfg)
    {
    for (size_t i = 0; i < pin_count; ++i) {
        gpio_config_t local_cfg = *cfg;
        local_cfg.pin_bit_mask = (1ULL << pins[i]);
        gpio_config(&local_cfg);
        gpio_isr_handler_add(pins[i], isr_func, isr_ctx);
    }
}

/* Helper: create and enable a pin glitch filter for a single GPIO.
   On success, *out_handle is set to the created handle. On failure, *out_handle
   is set to NULL (if non-NULL) and an error code is returned. */
static esp_err_t create_and_enable_pin_glitch_filter(gpio_num_t gpio, gpio_glitch_filter_handle_t *out_handle)
{
    if (out_handle) *out_handle = NULL;
    gpio_pin_glitch_filter_config_t cfg = {
        .clk_src = 0,
        .gpio_num = gpio,
    };

    gpio_glitch_filter_handle_t h = NULL;
    esp_err_t err = gpio_new_pin_glitch_filter(&cfg, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = gpio_glitch_filter_enable(h);
    if (err != ESP_OK) {
        // enable failed; clean up created filter
        gpio_del_glitch_filter(h);
        return err;
    }
    if (out_handle) *out_handle = h;
    return ESP_OK;
} 

static uint8_t button_state = 0xA5; // Initial "On" state. bitwise invert for "Off"

void IRAM_ATTR button_gpio_isr(void *arg) {
    isr_ctx_t *ctx = (isr_ctx_t *)arg;
    // Using hardware glitch filter; ISR must be short and non-blocking.
    gpio_set_level(13, button_state & 0x01); // Update test LED
    button_state ^= 0xFF; // Toggle state
    gpio_isr_msg_t msg = { 
        ctx->source_id,
        {0}, // target_id to be filled below 
        button_state
    };
    dispatcher_fill_targets(msg.target_id);
    switch(button_state){
        case(0x5A):
            msg.target_id[0] = TARGET_LOG;
            msg.target_id[1] = TARGET_USB_MSC;
            break;
        case(0xA5):
            msg.target_id[0] = TARGET_LOG;
            msg.target_id[1] = TARGET_USB_MSC;
            break;
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(ctx->queue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void IRAM_ATTR line_sensor_gpio_isr(void *arg) {
    isr_ctx_t *ctx = (isr_ctx_t *)arg;
    /* First-stage HW glitch filters (if present) reduce short spikes. As the
       next stage, we perform the original multi-sample software debounce here
       synchronously inside the ISR (this is blocking — see debounce func
       comments). This is the incremental behavior you requested before
       migrating to a task-based model. */
    uint8_t packed = 0;
    if (!debounce_line_sensor(ctx->pins, ctx->pin_count, &packed)) {
        return; // unstable, drop
    }

    gpio_isr_msg_t msg = { 
        ctx->source_id, 
        {0}, // target_id to be filled below
        packed 
    };
    dispatcher_fill_targets(msg.target_id);
    memcpy(msg.target_id, ctx->target_id, sizeof(msg.target_id));
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(ctx->queue, &msg, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}


// Hardcoded setup for button on GPIO9
void setup_button(void) {
    static gpio_num_t pins[1] = {0};
    static isr_ctx_t ctx = {
        .pins = pins,
        .pin_count = 1,
        .source_id = SOURCE_MSC_BUTTON,
    };
    ctx.queue = gpio_event_queue;

    gpio_config_t cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    register_gpio_isr(pins, 1, button_gpio_isr, &ctx, &cfg);

    // Create and enable hardware pin glitch filter for the button (centralized helper)
    esp_err_t err = create_and_enable_pin_glitch_filter(pins[0], &button_glitch_filter);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create pin glitch filter for GPIO%d: %d", pins[0], err);
        button_glitch_filter = NULL;
    }
}

// Hardcoded setup for line sensor on GPIO0-7
void setup_line_sensor(void) {
    static gpio_num_t pins[8] = {38,39,40,41,13,14,15,16};
    static isr_ctx_t ctx = {
        .pins = pins,
        .pin_count = 8,
        .source_id = SOURCE_LINE_SENSOR, // Example source ID
    };
    ctx.queue = gpio_event_queue;
    dispatcher_fill_targets(ctx.target_id);
    ctx.target_id[0] = TARGET_LINE_SENSOR_WINDOW;
   
    gpio_config_t cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    register_gpio_isr(pins, ctx.pin_count, line_sensor_gpio_isr, &ctx, &cfg);

    /* Create a pin glitch filter for each line sensor pin. If creation fails
       for a pin, log and leave that pin unfiltered for now. */
    for (size_t i = 0; i < ctx.pin_count; ++i) {
        esp_err_t ferr = create_and_enable_pin_glitch_filter(pins[i], &line_sensor_glitch_filters[i]);
        if (ferr != ESP_OK) {
            ESP_LOGW(TAG, "Line sensor: failed to create pin glitch filter for GPIO%d: %d", pins[i], ferr);
            line_sensor_glitch_filters[i] = NULL;
        }
    }
}

bool io_gpio_get_state(gpio_num_t gpio_num) {
    return gpio_get_level(gpio_num);
}

void io_gpio_event_task(void *arg) {
    gpio_isr_msg_t msg;
    uint8_t last_line_state = 0;
    bool last_line_state_valid = false;
    while (1) {
        if (xQueueReceive(gpio_event_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.source_id == SOURCE_LINE_SENSOR) {
                if (last_line_state_valid && msg.state == last_line_state) {
                    continue; // drop duplicate line sensor state
                }
                last_line_state = msg.state;
                last_line_state_valid = true;
            }
            dispatch_target_t ptr_targets[TARGET_MAX];
            dispatch_target_t val_targets[TARGET_MAX];
            dispatcher_fill_targets(ptr_targets);
            dispatcher_fill_targets(val_targets);

            size_t ptr_idx = 0;
            size_t val_idx = 0;
            for (size_t i = 0; i < TARGET_MAX; ++i) {
                dispatch_target_t t = msg.target_id[i];
                if (t == TARGET_MAX) continue;
                if (dispatcher_has_ptr_queue(t)) {
                    if (ptr_idx < TARGET_MAX) ptr_targets[ptr_idx++] = t;
                } else {
                    if (val_idx < TARGET_MAX) val_targets[val_idx++] = t;
                }
            }

            if (ptr_idx > 0) {
                dispatcher_pool_type_t pool_type = (msg.source_id == SOURCE_MSC_BUTTON)
                    ? DISPATCHER_POOL_CONTROL
                    : DISPATCHER_POOL_STREAMING;
                dispatcher_pool_send_params_t params = {
                    .type = pool_type,
                    .source = msg.source_id,
                    .targets = ptr_targets,
                    .data = &msg.state,
                    .data_len = 1,
                    .context = NULL
                };
                if (!dispatcher_pool_send_ptr_params(&params)) {
                    ESP_LOGW(TAG, "Pool send failed for source %d", msg.source_id);
                }
            }

            if (val_idx > 0) {
                ESP_LOGW(TAG, "Dropping value-path targets for source %d (no pointer queue)", msg.source_id);
            }
        }
    }
}
