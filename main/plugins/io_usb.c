#include "io_usb.h"
#include "dispatcher.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_cdc_acm.h"

#define USB_TX_QUEUE_LEN 10

// Queue for outgoing USB transmissions
static QueueHandle_t usb_tx_queue = NULL;

// Forward declaration
static void io_usb_tx_task(void *arg);

// Dispatcher handler — messages routed TO USB
static void io_usb_dispatcher_handler(const dispatcher_msg_t *msg)
{
    xQueueSend(usb_tx_queue, msg, 0);
}

void io_usb_init(void)
{
    // Create TX queue
    usb_tx_queue = xQueueCreate(USB_TX_QUEUE_LEN, sizeof(dispatcher_msg_t));

    // Register with dispatcher
    dispatcher_register_handler(TARGET_USB_CDC, io_usb_dispatcher_handler);

    // Start USB TX task
    xTaskCreate(io_usb_tx_task, "io_usb_tx_task", 4096, NULL, 9, NULL);

    // --- Your original USB_init() logic ---
    const tinyusb_config_t tusb_config = TINYUSB_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_config));

    const tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = tinyusb_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
}

// TX task — sends data OUT over USB
static void io_usb_tx_task(void *arg)
{
    dispatcher_msg_t msg;

    while (1) {
        if (xQueueReceive(usb_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {

            tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                       msg.data,
                                       msg.message_len);

            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
        }
    }
}

// RX callback — called by TinyUSB when data arrives
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    if (itf != TINYUSB_CDC_ACM_0) {
        return;
    }

    if (event->type == CDC_EVENT_RX) {

        dispatcher_msg_t msg = {0};

        esp_err_t ret = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                            msg.data,
                                            BUF_SIZE - 1,
                                            &msg.message_len);

        memset(msg.targets, TARGET_MAX, sizeof(msg.targets));
        if (ret == ESP_OK && msg.message_len > 0) {

            msg.source = SOURCE_USB_CDC;
            msg.targets[0] = TARGET_LIDAR_COORD;   // USB → LIDAR_COORD bridge

            BaseType_t hp = pdFALSE;
            dispatcher_send_from_isr(&msg, &hp);
            portYIELD_FROM_ISR(hp);
        }
    }
}