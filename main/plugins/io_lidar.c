#include "io_lidar.h"
#include "dispatcher.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define UART_TX_QUEUE_LEN 10
#define UART_EVENT_QUEUE_LEN 10

// Queues
static QueueHandle_t uart_tx_queue = NULL;
static QueueHandle_t uart_event_queue = NULL;

// Forward declarations
static void io_lidar_tx_task(void *arg);

// Dispatcher handler — messages routed TO UART
static void io_lidar_dispatcher_handler(const dispatcher_msg_t *msg)
{
    xQueueSend(uart_tx_queue, msg, 0);
}

void io_lidar_init(void)
{
    // Create TX queue
    uart_tx_queue = xQueueCreate(UART_TX_QUEUE_LEN, sizeof(dispatcher_msg_t));

    // UART config
    uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // Create UART event queue
    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    // Let the driver create the event queue
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_EXAMPLE_UART_PORT_NUM,
                                        BUF_SIZE * 2,
                                        0,
                                        UART_EVENT_QUEUE_LEN,
                                        &uart_event_queue,
                                        intr_alloc_flags));

    ESP_ERROR_CHECK(uart_param_config(CONFIG_EXAMPLE_UART_PORT_NUM, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(CONFIG_EXAMPLE_UART_PORT_NUM,
                                 CONFIG_EXAMPLE_UART_TXD,
                                 CONFIG_EXAMPLE_UART_RXD,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    // Register with dispatcher
    dispatcher_register_handler(TARGET_LIDAR_IO, io_lidar_dispatcher_handler);

    // Start TX task
    xTaskCreate(io_lidar_tx_task, "io_lidar_tx_task", 4096, NULL, 9, NULL);

    // Start RX event task
    xTaskCreate(io_lidar_event_task, "io_lidar_event_task", 4096, NULL, 10, NULL);


}

// RX event task — receives data FROM UART and sends it INTO dispatcher
void io_lidar_event_task(void *arg)
{
    uart_event_t event;

    while (1) {
        if (xQueueReceive(uart_event_queue, &event, portMAX_DELAY)) {

            if (event.type == UART_DATA) {
                dispatcher_msg_t msg = {0};

                msg.message_len = uart_read_bytes(CONFIG_EXAMPLE_UART_PORT_NUM,
                                                  msg.data,
                                                  BUF_SIZE - 1,
                                                  20 / portTICK_PERIOD_MS);
                memset(msg.targets, TARGET_MAX, sizeof(msg.targets));
                if (msg.message_len > 0) {
                    msg.source = SOURCE_LIDAR_IO;
                    msg.targets[0] = TARGET_LIDAR_COORD;   // UART → LIDAR_COORD bridge
                    dispatcher_send(&msg);
                }
            }
        }
    }
}

// TX task — sends data OUT over UART
static void io_lidar_tx_task(void *arg)
{
    dispatcher_msg_t msg = {0};

    while (1) {
        if (xQueueReceive(uart_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            uart_write_bytes(CONFIG_EXAMPLE_UART_PORT_NUM,
                             (const char *)msg.data,
                             msg.message_len);
        }
    }
}