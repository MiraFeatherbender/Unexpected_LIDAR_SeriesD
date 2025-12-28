/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_cdc_acm.h"
#include "sdkconfig.h"
#include "esp_log.h"

/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)

#define BUF_SIZE (1024)
#define QUEUE_LEN 10

static const char *TAG = "USB_UART_BRIDGE";

// Function Prototypes
static void USB_init(void);
static void uart_init(void);
static void uart_event_task(void *pvParameters);
static void dispatcher_task(void *arg);
static void usb_tx_task(void *arg);
static void uart_tx_task(void *arg);
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event);

// enums and structs for communication state and context
typedef enum {
    USB_TARGET = TINYUSB_CDC_ACM_0,
    UART_TARGET = ECHO_UART_PORT_NUM + 1
} commTarget_t;

typedef enum {
    INTERNAL = 0,
    EXTERNAL = 1
} commSource_t;

typedef struct {
    uint8_t data[BUF_SIZE];
    size_t message_len;
    commTarget_t target;
    commSource_t source;
} commContext_t;

// Task handles
static TaskHandle_t dispatcher_task_handle = NULL;
static TaskHandle_t usb_tx_task_handle = NULL;
static TaskHandle_t uart_tx_task_handle = NULL;

// Queues
static QueueHandle_t dispatch_queue = NULL;
static QueueHandle_t usb_tx_queue = NULL;
static QueueHandle_t uart_tx_queue = NULL;
static QueueHandle_t uart_event_queue = NULL;

void app_main(void)
{
    // Initialize USB
    USB_init();

    // Initialize the UART
    uart_init();

    // Create queues
    dispatch_queue = xQueueCreate(QUEUE_LEN, sizeof(commContext_t));
    usb_tx_queue   = xQueueCreate(QUEUE_LEN, sizeof(commContext_t));
    uart_tx_queue  = xQueueCreate(QUEUE_LEN, sizeof(commContext_t));

    // Create tasks to echo data

    xTaskCreate(dispatcher_task, "dispatcher_task", ECHO_TASK_STACK_SIZE, NULL, 10, &dispatcher_task_handle);
    xTaskCreate(usb_tx_task, "usb_tx_task", ECHO_TASK_STACK_SIZE, NULL, 9, &usb_tx_task_handle);
    xTaskCreate(uart_tx_task, "uart_tx_task", ECHO_TASK_STACK_SIZE, NULL, 9, &uart_tx_task_handle);
    xTaskCreate(uart_event_task, "uart_event_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    while (1) {
        // Waiting for UART event.
        if (xQueueReceive(uart_event_queue, (void *)&event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    commContext_t msg = {0};
                    msg.message_len = uart_read_bytes(ECHO_UART_PORT_NUM,
                                                      msg.data,
                                                      BUF_SIZE - 1,
                                                      20 / portTICK_PERIOD_MS);
                    if (msg.message_len > 0) {
                        msg.source = EXTERNAL;      // From UART
                        msg.target = USB_TARGET;    // Route to USB
                        xQueueSend(dispatch_queue, &msg, 0);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    if (itf != TINYUSB_CDC_ACM_0) {
        return;
    }

    if (event->type == CDC_EVENT_RX) {
        commContext_t msg = {0};
        esp_err_t ret = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                            msg.data,
                                            BUF_SIZE - 1,
                                            &msg.message_len);
        if (ret == ESP_OK && msg.message_len > 0) {
            msg.source = EXTERNAL;      // From USB
            msg.target = UART_TARGET;   // Route to UART
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(dispatch_queue, &msg, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

static void dispatcher_task(void *arg)
{
    commContext_t msg;

    while (1) {
        // Wait for any routed message
        if (xQueueReceive(dispatch_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.target) {
                case USB_TARGET:
                    // Route to USB TX queue
                    xQueueSend(usb_tx_queue, &msg, 0);
                    break;

                case UART_TARGET:
                    // Route to UART TX queue
                    xQueueSend(uart_tx_queue, &msg, 0);
                    break;

                default:
                    break;
            }
        }
    }
}

static void usb_tx_task(void *arg)
{
    commContext_t msg;

    while (1) {
        if (xQueueReceive(usb_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, msg.data, msg.message_len);
            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0); // 0 = no wait
        }
    }
}

static void uart_tx_task(void *arg)
{
    commContext_t msg;

    while (1) {
        if (xQueueReceive(uart_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)msg.data, msg.message_len);
        }
    }
}

static void USB_init(void)
{
    // Initialize and install the USB-CDC driver
    const tinyusb_config_t tusb_config = TINYUSB_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_config));
    // Initialize the USB-CDC serial port
    const tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = tinyusb_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
}

static void uart_init(void)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM,
                                        BUF_SIZE * 2,
                                        0,
                                        QUEUE_LEN,
                                        &uart_event_queue,
                                        intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM,
                                 ECHO_TEST_TXD,
                                 ECHO_TEST_RXD,
                                 ECHO_TEST_RTS,
                                 ECHO_TEST_CTS));
}