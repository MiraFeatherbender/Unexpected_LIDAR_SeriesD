#include "io_usb_msc.h"
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "tinyusb.h"
#include "tinyusb_msc.h"

#include "esp_partition.h"



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

static wl_handle_t wl_handle = WL_INVALID_HANDLE;
static tinyusb_msc_storage_handle_t storage_hdl = NULL;

// MSC event callback — handles mount/unmount events
static void msc_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg) {
    if (!event || event->id != TINYUSB_MSC_EVENT_MOUNT_COMPLETE) return;
    dispatcher_msg_t msg = {
        .source = SOURCE_USB_MSC,
        .message_len = 1,
        .data = {0}
    };
    memset(msg.targets, TARGET_MAX, sizeof(msg.targets));
    msg.targets[0] = TARGET_RGB;
    switch (event->mount_point) {
        case TINYUSB_MSC_STORAGE_MOUNT_APP:
            // PC ejected/unmounted, app regains access
            msg.data[0] = 0x5A;
            dispatcher_send(&msg);
            return;
        case TINYUSB_MSC_STORAGE_MOUNT_USB:
            // PC mounted, app lost access
            msg.data[0] = 0xA5;
            dispatcher_send(&msg);
            return;
        default:
            return;
    }
}


void io_usb_msc_init(void){

    // Create TX queue
    msc_tx_queue = xQueueCreate(USB_MSC_TX_QUEUE_LEN, sizeof(dispatcher_msg_t));

    // Register with dispatcher
    dispatcher_register_handler(TARGET_USB_MSC, io_usb_msc_dispatcher_handler);

    // Start MSC task
    xTaskCreate(io_usb_msc_task, "io_usb_msc_task", 4096, NULL, 9, NULL);

    // Mount the partition with wear leveling
    const esp_partition_t *data_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        "fatfs"
    );
    if (!data_partition) {
        ESP_LOGE("MSC", "Failed to find FATFS partition. Check the partition table.");
        return;
    }
    ESP_ERROR_CHECK(wl_mount(data_partition, &wl_handle));

    // Configure TinyUSB MSC storage
    const tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
        .medium.wl_handle = wl_handle,
        .fat_fs = {
            .base_path = "/data",
        },
    };
    tinyusb_msc_new_storage_spiflash(&storage_cfg, &storage_hdl);
    msc_enabled = false; // Start in APP mode (not enabled for PC)
    
    // Register MSC event callback for mount/unmount detection
    tinyusb_msc_set_storage_callback(msc_event_cb, NULL);

 
}


bool io_usb_msc_is_enabled(void)
{
    return msc_enabled;
}


void io_usb_msc_enable(void)
{
    if (!msc_enabled) {
        tinyusb_msc_set_storage_mount_point(storage_hdl, TINYUSB_MSC_STORAGE_MOUNT_USB);
        msc_enabled = true;
    }
}

void io_usb_msc_disable(void)
{
    if (msc_enabled) {
        tinyusb_msc_set_storage_mount_point(storage_hdl, TINYUSB_MSC_STORAGE_MOUNT_APP);
        msc_enabled = false;
    }
}
// MSC task — toggles MSC state on every button event
static void io_usb_msc_task(void *arg)
{
    dispatcher_msg_t msg;
    while (1) {
        if (xQueueReceive(msc_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            // Toggle MSC state on any valid button event
            if (msg.data[0] == 0xA5 || msg.data[0] == 0x5A) {
                if (msc_enabled) {
                    io_usb_msc_disable();
                } else {
                    io_usb_msc_enable();
                }
            }
        }
    }
}
