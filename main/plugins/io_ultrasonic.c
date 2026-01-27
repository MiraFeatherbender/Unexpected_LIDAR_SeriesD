#include "io_ultrasonic.h"
#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "dispatcher_module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include <string.h>

static const char *TAG = "io_ultrasonic";

#define ULTRASONIC_MIN_PULSE_US  (20)     /* Minimum valid TX pulse (us) */
#define ULTRASONIC_MIN_PULSE_US  (20)     /* Minimum valid TX pulse (us) */

/* Range clipping for distances (mm) */
#define ULTRASONIC_MIN_MM  (30)  /* Clip anything < 30mm to 30mm */
#define ULTRASONIC_MAX_MM  (450) /* Clip anything > 450mm to 450mm */

/* Maximum echo round-trip time (us) for ULTRASONIC_MAX_MM
 * MAX_DELTA_US = (2 * max_mm * 1e6) / speed_of_sound_mm_per_s
 */
#define ULTRASONIC_MAX_DELTA_US (((2ULL * ULTRASONIC_MAX_MM) * 1000000ULL) / 343000ULL)
#define ULTRASONIC_DELTA_MARGIN_US 200 /* safety margin (us) for stale-rise timeout */

/* Pin assignments */
#define ULTRASONIC_TX_PIN (42)
#define ULTRASONIC_RX_PIN (12)

static esp_timer_handle_t tx_pulse_timer = NULL;

/* ISR -> task message and queue for RX captures */
#define ULTRASONIC_EVENT_QUEUE_LEN 32

/* Capture struct sent from ISR: pin level (0/1) + timestamp in microseconds */
typedef struct {
    uint8_t level;    /* GPIO level: 0 = low, 1 = high */
    uint64_t ts_us;   /* timestamp in microseconds (esp_timer_get_time()) */
} ultrasonic_capture_t;

/* Queue of capture structs */
static QueueHandle_t ultrasonic_event_queue = NULL;

/* Forward declarations */
static void ultrasonic_process_msg(const dispatcher_msg_t *msg);
static void ultrasonic_step_frame(void);
static void ultrasonic_event_task(void *arg);

/* Helper: median of three unsigned 32-bit values */
static inline uint32_t median3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t tmp;
    if (a > b) { tmp = a; a = b; b = tmp; }
    if (b > c) { tmp = b; b = c; c = tmp; }
    if (a > b) { tmp = a; a = b; b = tmp; }
    return b;
} 


/* Timer callback to clear TX pin when pulse end occurs */
static void tx_pulse_timer_cb(void *arg)
{
    (void)arg;
    /* Clear TX pin; short and non-blocking */
    gpio_set_level(ULTRASONIC_TX_PIN, 0);
} 

/* RX GPIO ISR: capture current esp_timer_get_time() timestamp and enqueue it to the event queue */
IRAM_ATTR static void ultrasonic_rx_gpio_isr(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    if (!queue) return;

    ultrasonic_capture_t cap;
    cap.ts_us = (uint64_t)esp_timer_get_time(); /* microseconds since boot */
    cap.level = (uint8_t)gpio_get_level(ULTRASONIC_RX_PIN);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(queue, &cap, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
} 

static dispatcher_module_t ultrasonic_mod = {
    .name = "io_ultrasonic",
    .target = TARGET_ULTRASONIC,
    .queue_len = 8,
    .stack_size = 4096,
    .task_prio = 5,
    .process_msg = ultrasonic_process_msg,
    .step_frame = ultrasonic_step_frame,
    .step_ms = 100,
    .queue = NULL,
    .next_step = 0,
    .last_queue_warn = 0
};

void io_ultrasonic_init(void)
{
    ESP_LOGI(TAG, "Initializing io_ultrasonic module");

    /* Start dispatcher-managed module (creates ptr queue & task)
     * This ensures we conform to the project's dispatcher contract before
     * adding hardware-specific logic. */
    if (dispatcher_module_start(&ultrasonic_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for io_ultrasonic");
        return;
    }

    /* No GPTimer in use; using esp_timer_get_time() for timestamps and
     * an esp_timer one-shot to clear the TX pin after the configured pulse */
    esp_err_t err = ESP_OK;

    /* Create an esp_timer one-shot used to clear TX after the pulse width */
    const esp_timer_create_args_t tx_timer_args = {
        .callback = tx_pulse_timer_cb,
        .arg = NULL,
        .name = "ultrasonic_tx_pulse"
    };
    err = esp_timer_create(&tx_timer_args, &tx_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_timer_create (tx_pulse_timer) failed: %d", err);
        tx_pulse_timer = NULL;
    }

    /* Note: no GPTimer used; using esp_timer for scheduling pulse end and
     * esp_timer_get_time() for timestamps. */

    /* Configure TX GPIO (initially low). */
    gpio_config_t tx_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << ULTRASONIC_TX_PIN),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&tx_cfg);
    gpio_set_level(ULTRASONIC_TX_PIN, 0);

    /* Configure RX GPIO (input, trigger on both edges). */
    gpio_config_t rx_cfg = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ULTRASONIC_RX_PIN),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&rx_cfg);

    /* Create an ISR-safe queue for capture structs */
    ultrasonic_event_queue = xQueueCreate(ULTRASONIC_EVENT_QUEUE_LEN, sizeof(ultrasonic_capture_t));
    if (!ultrasonic_event_queue) {
        ESP_LOGE(TAG, "Failed to create ultrasonic event queue");
    } else {
        /* Install GPIO ISR service and add handler (pass queue handle directly) */
        gpio_install_isr_service(0);
        esp_err_t rc = gpio_isr_handler_add(ULTRASONIC_RX_PIN, ultrasonic_rx_gpio_isr, ultrasonic_event_queue);
        if (rc == ESP_OK) {
            ESP_LOGI(TAG, "RX ISR handler installed for GPIO %d (both edges)", ULTRASONIC_RX_PIN);
        } else {
            ESP_LOGW(TAG, "Failed to install RX ISR handler: %d", rc);
        }

        /* Create task to process captured timestamps */
        xTaskCreate(ultrasonic_event_task, "ultrasonic_event_task", 4096, NULL, 9, NULL);
    }

    ESP_LOGI(TAG, "Using esp_timer for timestamps; pulse width = %u us", ULTRASONIC_MIN_PULSE_US);
}



void io_ultrasonic_trigger_once(void)
{
    /* For now, call step_frame to exercise the dispatcher publishing path.
     * Later this will trigger the measurement. */
    ultrasonic_step_frame();
}

static void ultrasonic_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg) return;
    ESP_LOGI(TAG, "ultrasonic process_msg: source=%d, len=%u", msg->source, (unsigned)msg->message_len);
    /* Handle control messages (if any) here in the future */
}

/* Event task: converts captured ticks into distance mm and publishes via dispatcher
 * Implements a 3-sample median filter and range clipping. */
static void ultrasonic_event_task(void *arg)
{
    (void)arg;
    ultrasonic_capture_t cap;
    uint64_t last_rising_ts = 0;

    uint16_t window[3] = {0};
    int win_idx = 0;
    int win_count = 0;

    while (1) {
        if (xQueueReceive(ultrasonic_event_queue, &cap, portMAX_DELAY) != pdTRUE) {
            continue; /* nothing to do */
        }

        uint64_t ts = cap.ts_us;
        /* Clear stale rising timestamp if it's older than the max expected echo
         * plus a margin. This prevents a lost falling edge from blocking future
         * measurements indefinitely. */
        if (last_rising_ts != 0 && ts > last_rising_ts &&
            (ts - last_rising_ts) > (ULTRASONIC_MAX_DELTA_US + ULTRASONIC_DELTA_MARGIN_US)) {
            ESP_LOGD(TAG, "Clearing stale rising ts (last=%llu now=%llu)", (unsigned long long)last_rising_ts, (unsigned long long)ts);
            last_rising_ts = 0;
        }

        switch (cap.level) {
        case 1: /* Rising edge */
            last_rising_ts = ts;
            ESP_LOGD(TAG, "RX rising edge ts=%llu", (unsigned long long)ts);
            break;

        case 0: { /* Falling edge (early-exit validation) */
            if (last_rising_ts == 0) {
                ESP_LOGD(TAG, "Ignoring falling edge without prior rising edge (falling=%llu)", (unsigned long long)ts);
                break;
            }
            if (ts <= last_rising_ts) {
                ESP_LOGD(TAG, "Ignoring falling edge with non-positive delta (rising=%llu falling=%llu)",
                         (unsigned long long)last_rising_ts, (unsigned long long)ts);
                break;
            }

            uint64_t delta_us = ts - last_rising_ts;

            /* Drop echoes that exceed the reliable max distance to avoid
             * publishing unreliable readings. */
            if (delta_us > ULTRASONIC_MAX_DELTA_US) {
                ESP_LOGD(TAG, "Dropping echo: delta_us=%llu > max=%llu", (unsigned long long)delta_us, (unsigned long long)ULTRASONIC_MAX_DELTA_US);
                last_rising_ts = 0;
                break;
            }

            /* Distance (mm) = (delta_us * speed_of_sound_mm_per_s) / 2 / 1e6
             * speed_of_sound = 343000 mm/s */
            uint16_t distance_mm = (uint16_t)((delta_us * 343000ULL) / 2000000ULL);

            /* Insert into sliding window */
            window[win_idx] = distance_mm;
            win_idx = (win_idx + 1) % 3;
            if (win_count < 3) ++win_count;

            /* Require a full 3-sample window; otherwise early-exit */
            if (win_count != 3) {
                /* Not enough samples yet; reset rising timestamp and continue */
                last_rising_ts = 0;
                break;
            }

            /* Compute median, clip to configured range, then publish */
            uint16_t med = median3(window[0], window[1], window[2]);
            if (med < ULTRASONIC_MIN_MM) med = ULTRASONIC_MIN_MM;
            if (med > ULTRASONIC_MAX_MM) med = ULTRASONIC_MAX_MM;

            /* Publish distance via dispatcher pointer-pool (TARGET_LOG only) */
            dispatch_target_t targets[TARGET_MAX];
            dispatcher_fill_targets(targets);
            targets[0] = TARGET_LOG;

            IO_ULTRASONIC_PUBLISH_MM(med, targets);

            /* Reset last rising timestamp so we require a new rising edge */
            last_rising_ts = 0;
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown GPIO level in capture: %u", (unsigned)cap.level);
            break;
        }
    }
}

static void ultrasonic_step_frame(void)
{
    gpio_set_level(ULTRASONIC_TX_PIN, 1);

    /* Start one-shot to clear TX after configured pulse width */
    esp_timer_start_once(tx_pulse_timer, ULTRASONIC_MIN_PULSE_US);

}
