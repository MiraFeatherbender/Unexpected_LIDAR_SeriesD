#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "io_gpio.h"
#include "soc/gpio_reg.h"
#include "driver/gpio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "io_gpio";

static QueueHandle_t gpio_event_queue = NULL;

void io_gpio_init(void) {
    gpio_event_queue = xQueueCreate(IO_GPIO_EVENT_QUEUE_LEN, sizeof(gpio_isr_msg_t));   
    gpio_install_isr_service(0); 
    setup_button();
    setup_line_sensor();
    // setup_test_led();

    // Start GPIO event task
    xTaskCreate(io_gpio_event_task, "io_gpio_event_task", 4096, NULL, 9, NULL);

}

// Helper: pack pin states from all_gpio for a set
static inline uint8_t pack_gpio_set(const gpio_num_t *pins, size_t pin_count, uint64_t all_gpio) {
    uint8_t state = 0;
    for (size_t i = 0; i < pin_count; ++i) {
        state <<= 1;
        state |= (all_gpio >> pins[i]) & 0x01;
    }
    return state;
}

static inline uint64_t read_all_gpio(void) {
    uint32_t all_lower_gpio = REG_READ(GPIO_IN_REG);
    uint32_t all_upper_gpio = REG_READ(GPIO_IN1_REG);
    return ((uint64_t)all_upper_gpio << 32) | all_lower_gpio;
}

// Simple debounce function for a button pin
bool debounce_button(gpio_num_t pin) {
    uint16_t state = 0xFFFF;
    for (int count = 0; count < 32; count++) {
        state = (state << 1) | gpio_get_level(pin);
        esp_rom_delay_us(500); // 500 microseconds delay
        if ((state & 0xFFFF) == 0) {
            // 8 consecutive zeros: button is stably LOW
            return true;
        }
    }
    // Not stable low
    return false;
}

// Debounce packed line sensor state
static bool debounce_line_sensor(const gpio_num_t *pins, size_t pin_count, uint8_t *out_state) {
    if (!out_state) return false;
    uint32_t history = 0;
    for (int i = 0; i < 32; ++i) {
        esp_rom_delay_us(200);
        uint8_t cur = pack_gpio_set(pins, pin_count, read_all_gpio());
        history = (history << 8) | cur;
        if (i >= 3) {
            uint32_t pattern = ((uint32_t)cur) * 0x01010101u; // 4 identical bytes
            if ((history ^ pattern) == 0) {
                *out_state = cur;
                return true;
            }
        }
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

static uint8_t button_state = 0xA5; // Initial "On" state. bitwise invert for "Off"

void IRAM_ATTR button_gpio_isr(void *arg) {
    isr_ctx_t *ctx = (isr_ctx_t *)arg;
    if (!debounce_button(ctx->pins[0])) {
        return;
    }
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
    uint8_t packed = 0;
    if (!debounce_line_sensor(ctx->pins, ctx->pin_count, &packed)) {
        return;
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
    static gpio_num_t pins[1] = {12};
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
}

// Hardcoded setup for line sensor on GPIO0-7
void setup_line_sensor(void) {
    static gpio_num_t pins[8] = {38,39,40,41,13,14,15,16};
    static isr_ctx_t ctx = {
        .pins = pins,
        .pin_count = 8,
        .source_id = SOURCE_LINE_SENSOR, // Example source ID
        .target_id[0] = TARGET_LINE_SENSOR_WINDOW,
    };
    ctx.queue = gpio_event_queue;
   
    gpio_config_t cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    register_gpio_isr(pins, 8, line_sensor_gpio_isr, &ctx, &cfg);
}

bool io_gpio_get_state(gpio_num_t gpio_num) {
    return gpio_get_level(gpio_num);
}


void setup_test_led(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << 13),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
    gpio_set_level(13, 0); // Start with LED off

    cfg.pin_bit_mask = (1ULL << 14);
    cfg.mode = GPIO_MODE_INPUT;
    gpio_config(&cfg);
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
                if (!dispatcher_pool_send_ptr(pool_type,
                                              msg.source_id,
                                              ptr_targets,
                                              &msg.state,
                                              1,
                                              NULL)) {
                    ESP_LOGW(TAG, "Pool send failed for source %d", msg.source_id);
                }
            }

            if (val_idx > 0) {
                dispatcher_msg_t dmsg = {
                    .source = msg.source_id,
                    .message_len = 1,
                    .data = { msg.state },
                    .context = NULL
                };
                memcpy(dmsg.targets, val_targets, sizeof(dmsg.targets));
                dispatcher_send(&dmsg);
            }
        }
    }
}
