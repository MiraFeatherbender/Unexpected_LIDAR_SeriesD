
#pragma once
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define IO_GPIO_EVENT_QUEUE_LEN 8

void io_gpio_init(void);
void register_gpio_isr(
    const gpio_num_t *pins, size_t pin_count, 
    gpio_isr_t isr_func, void *isr_ctx, gpio_config_t *cfg);
bool debounce_button(gpio_num_t pin);
void setup_button(void);
void setup_line_sensor(void);
void io_gpio_event_task(void *arg);
void button_gpio_isr(void *arg);
void line_sensor_gpio_isr(void *arg);
bool io_gpio_get_state(gpio_num_t gpio_num);
void setup_test_led(void);

typedef struct {
    const gpio_num_t *pins;
    size_t pin_count;
    dispatch_source_t source_id;
    dispatch_target_t target_id[TARGET_MAX];
    QueueHandle_t queue;
} isr_ctx_t;

typedef struct {
    dispatch_source_t source_id;
    dispatch_target_t target_id[TARGET_MAX];
    uint8_t state;
} gpio_isr_msg_t;

bool io_gpio_get_state(gpio_num_t gpio_num);
