#include "io_lidar.h"
#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "dispatcher_module.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define UART_EVENT_QUEUE_LEN 10

// Queues
static QueueHandle_t uart_event_queue = NULL;
static QueueHandle_t uart_tx_ptr_queue = NULL;

// Forward declarations
static void io_lidar_tx_task(void *arg);

void io_lidar_init(void)
{
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

    // Register pointer queue with dispatcher
    uart_tx_ptr_queue = dispatcher_ptr_queue_create_register(TARGET_LIDAR_IO, 10);
    if (!uart_tx_ptr_queue) {
        ESP_LOGE("io_lidar", "Failed to create pointer queue for LIDAR TX");
    }

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
                ESP_LOGI("io_lidar", "RX %u bytes", (unsigned)event.size);
                uint8_t tmp_buf[BUF_SIZE] = {0};
                int len = uart_read_bytes(CONFIG_EXAMPLE_UART_PORT_NUM,
                                          tmp_buf,
                                          BUF_SIZE - 1,
                                          20 / portTICK_PERIOD_MS);
                if (len > 0) {
                    dispatch_target_t targets[TARGET_MAX];
                    dispatcher_fill_targets(targets);
                    targets[0] = TARGET_LIDAR_COORD;   // UART → LIDAR_COORD bridge
                    dispatcher_pool_send_params_t params = {
                        .type = DISPATCHER_POOL_STREAMING,
                        .source = SOURCE_LIDAR_IO,
                        .targets = targets,
                        .data = tmp_buf,
                        .data_len = (size_t)len,
                        .context = NULL
                    };
                    dispatcher_pool_send_ptr_params(&params);
                }
            }
        }
    }
}

// TX task — sends data OUT over UART
static void io_lidar_tx_task(void *arg)
{
    while (1) {
        pool_msg_t *pmsg = NULL;
        if (xQueueReceive(uart_tx_ptr_queue, &pmsg, portMAX_DELAY) == pdTRUE) {
            const dispatcher_msg_ptr_t *msg = dispatcher_pool_get_msg_const(pmsg);
            if (msg && msg->data && msg->message_len > 0) {
                uart_write_bytes(CONFIG_EXAMPLE_UART_PORT_NUM,
                                 (const char *)msg->data,
                                 msg->message_len);
            }
            ESP_LOGI("io_lidar", "TX %02X %02X", (unsigned)msg->data[0], (unsigned)msg->data[1]);
            dispatcher_pool_msg_unref(pmsg);
        }
    }
}