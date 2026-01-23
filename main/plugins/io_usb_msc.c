#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_partition.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "dispatcher.h"
#include "dispatcher_module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define BASE_PATH "/data"

static const char *TAG = "usb_msc";

// MSC storage handle
static tinyusb_msc_storage_handle_t storage_hdl = NULL;
static wl_handle_t wl_handle = WL_INVALID_HANDLE;
static bool usb_stack_started = false;

// MSC state
static bool msc_enabled = false;
#define USB_MSC_TX_QUEUE_LEN 4

// Forward declarations
static void io_usb_msc_process_msg(const dispatcher_msg_t *msg);
static void msc_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg);
static esp_err_t usb_stack_enable(void);
static esp_err_t usb_stack_disable(void);

static dispatcher_module_t io_usb_msc_mod = {
    .name = "io_usb_msc_task",
    .target = TARGET_USB_MSC,
    .queue_len = USB_MSC_TX_QUEUE_LEN,
    .stack_size = 4096,
    .task_prio = 9,
    .process_msg = io_usb_msc_process_msg,
    .step_frame = NULL,
    .step_ms = 0,
    .queue = NULL
};

/* Pointer queue/task handled by dispatcher_module_start via io_usb_msc_mod */



static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    // ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}


static void msc_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg) {
    if (!event || event->id != TINYUSB_MSC_EVENT_MOUNT_COMPLETE) return;
    switch (event->mount_point) {
        case TINYUSB_MSC_STORAGE_MOUNT_APP:
            return;
        case TINYUSB_MSC_STORAGE_MOUNT_USB:
            return;
        default:
            return;
    }
}

// MSC state helpers
bool io_usb_msc_is_enabled(void) { return msc_enabled; }
static void io_usb_msc_enable(void) {
    if (!msc_enabled) {
        tinyusb_msc_set_storage_mount_point(storage_hdl, TINYUSB_MSC_STORAGE_MOUNT_USB);
        msc_enabled = true;
    }
}
static void io_usb_msc_disable(void) {
    if (msc_enabled) {
        tinyusb_msc_set_storage_mount_point(storage_hdl, TINYUSB_MSC_STORAGE_MOUNT_APP);
        msc_enabled = false;
    }
}

static esp_err_t usb_stack_enable(void)
{
    if (usb_stack_started) return ESP_OK;
    ESP_LOGI(TAG, "USB MSC enable");

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) return err;

    io_usb_msc_enable();
    usb_stack_started = true;
    return ESP_OK;
}

static esp_err_t usb_stack_disable(void)
{
    if (!usb_stack_started) return ESP_OK;
    ESP_LOGI(TAG, "USB MSC disable");

    io_usb_msc_disable();
    esp_err_t err = tinyusb_driver_uninstall();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "tinyusb_driver_uninstall failed: %s", esp_err_to_name(err));
    }
    usb_stack_started = false;
    return ESP_OK;
}

static void io_usb_msc_process_msg(const dispatcher_msg_t *msg)
{
    if (!msg) return;
    if (msg->data[0] == 0xA5 || msg->data[0] == 0x5A) {
        if (usb_stack_started) {
            usb_stack_disable();
        } else {
            usb_stack_enable();
        }
    }
}

// Call this from your main to initialize USB MSC
void io_usb_msc_init(void)
{
    ESP_LOGI(TAG, "Initializing storage (FATFS only)...");
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
        .medium.wl_handle = wl_handle,
        .fat_fs = { .base_path = BASE_PATH },
    };
    ESP_ERROR_CHECK(tinyusb_msc_new_storage_spiflash(&storage_cfg, &storage_hdl));
    ESP_ERROR_CHECK(tinyusb_msc_set_storage_callback(msc_event_cb, NULL));
    io_usb_msc_disable();

    // Start dispatcher module pointer handling
    if (dispatcher_module_start(&io_usb_msc_mod) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start dispatcher module for io_usb_msc");
        return;
    }

#define ENABLE_USB_MSC_INIT 0
#if ENABLE_USB_MSC_INIT
    ESP_ERROR_CHECK(usb_stack_enable());
#endif

}

