#include "io_usb_msc.h"
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "tinyusb.h"
#include "tinyusb_msc.h"
#include "esp_log.h"

#define TAG "io_usb_msc"
#define USB_MSC_TX_QUEUE_LEN 4

static QueueHandle_t msc_tx_queue = NULL;
static bool msc_enabled = false;

// Forward declaration
static void io_usb_msc_task(void *arg);

// Dispatcher handler — messages routed TO MSC
static void io_usb_msc_dispatcher_handler(const dispatcher_msg_t *msg)
{
    xQueueSend(msc_tx_queue, msg, 0);
}

void io_usb_msc_init(void)
{
    // Create TX queue
    msc_tx_queue = xQueueCreate(USB_MSC_TX_QUEUE_LEN, sizeof(dispatcher_msg_t));

    // Register with dispatcher
    dispatcher_register_handler(TARGET_USB_MSC, io_usb_msc_dispatcher_handler);

    // Start MSC task
    xTaskCreate(io_usb_msc_task, "io_usb_msc_task", 4096, NULL, 9, NULL);

    //tusb_msc_storage_init();
}

void io_usb_msc_enable(void)
{
    msc_enabled = true;
    // Actual enable logic (mount FATFS, enable MSC interface)
}

void io_usb_msc_disable(void)
{
    msc_enabled = false;
    // Actual disable logic (unmount FATFS, disable MSC interface)
}

bool io_usb_msc_is_enabled(void)
{
    return msc_enabled;
}

// MSC task — handles messages (expand as needed)
static void io_usb_msc_task(void *arg)
{
    dispatcher_msg_t msg;
    while (1) {
        if (xQueueReceive(msc_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            // Handle MSC-specific messages here
            // e.g., enable/disable, status, etc.
        }
    }
}
