#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_partition.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "tinyusb_cdc_acm.h"
#include "dispatcher.h"

#define BASE_PATH "/data"

static const char *TAG = "usb_cdc_msc";
static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

// MSC storage handle
static tinyusb_msc_storage_handle_t storage_hdl = NULL;
static wl_handle_t wl_handle = WL_INVALID_HANDLE;

// MSC state
static bool msc_enabled = false;
#define USB_MSC_TX_QUEUE_LEN 4
static QueueHandle_t msc_tx_queue = NULL;

// Forward declarations
static void io_usb_msc_task(void *arg);
static void msc_event_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg);

/**
 * @brief Application Queue
 */
static QueueHandle_t app_queue;
typedef struct {
    uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];     // Data buffer
    size_t buf_len;                                     // Number of bytes received
    uint8_t itf;                                        // Index of CDC device interface
} app_message_t;

/**
 * @brief CDC device RX callback
 *
 * CDC device signals, that new data were received
 *
 * @param[in] itf   CDC device index
 * @param[in] event CDC event type
 */
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    if (event->type == CDC_EVENT_RX) {
        dispatcher_msg_t msg = {0};
        size_t rx_size = 0;
        esp_err_t ret = tinyusb_cdcacm_read(itf, rx_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
        if (ret == ESP_OK && rx_size > 0) {
            msg.source = SOURCE_USB_CDC;
            msg.targets[0] = TARGET_LIDAR_COORD; // or set as needed
            msg.message_len = rx_size;
            memcpy(msg.data, rx_buf, rx_size);
            dispatcher_send(&msg);
        }
    }
}

/**
 * @brief CDC device line change callback
 *
 * CDC device signals, that the DTR, RTS states changed
 *
 * @param[in] itf   CDC device index
 * @param[in] event CDC event type
 */
void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    int dtr = event->line_state_changed_data.dtr;
    int rts = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "Line state changed on channel %d: DTR:%d, RTS:%d", itf, dtr, rts);
}

static bool file_exists(const char *file_path)
{
    struct stat buffer;
    return stat(file_path, &buffer) == 0;
}

static void file_operations(void)
{
    const char *directory = "/data/esp";
    const char *file_path = "/data/esp/test.txt";

    struct stat s = {0};
    bool directory_exists = stat(directory, &s) == 0;
    if (!directory_exists) {
        if (mkdir(directory, 0775) != 0) {
            ESP_LOGE(TAG, "mkdir failed with errno: %s", strerror(errno));
        }
    }

    if (!file_exists(file_path)) {
        ESP_LOGI(TAG, "Creating file");
        FILE *f = fopen(file_path, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
        fprintf(f, "Hello World!\n");
        fclose(f);
    }

    FILE *f;
    ESP_LOGI(TAG, "Reading file");
    f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);
}

static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}


// Dispatcher handler for USB CDC TX
static void io_usb_dispatcher_handler(const dispatcher_msg_t *msg)
{
    tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, msg->data, msg->message_len);
    esp_err_t err = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CDC ACM write flush error: %s", esp_err_to_name(err));
    }
}

// Dispatcher handler for MSC toggle (button events)
static void io_usb_msc_dispatcher_handler(const dispatcher_msg_t *msg)
{
    xQueueSend(msc_tx_queue, msg, 0);
}
// MSC event callback — notifies RGB on mount/unmount
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

// MSC state helpers
static bool io_usb_msc_is_enabled(void) { return msc_enabled; }
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

// Call this from your main to initialize USB CDC+MSC
void io_usb_cdc_msc_init(void)
{
    ESP_LOGI(TAG, "Initializing storage...");
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
        .medium.wl_handle = wl_handle,
        .fat_fs = { .base_path = BASE_PATH },
    };
    ESP_ERROR_CHECK(tinyusb_msc_new_storage_spiflash(&storage_cfg, &storage_hdl));

    // Do file operations before enabling USB MSC for host
    file_operations();

    ESP_LOGI(TAG, "USB Composite initialization");
    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &tinyusb_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
        TINYUSB_CDC_ACM_0,
        CDC_EVENT_LINE_STATE_CHANGED,
        &tinyusb_cdc_line_state_changed_callback));

    // MSC dispatcher queue and handler
    msc_tx_queue = xQueueCreate(USB_MSC_TX_QUEUE_LEN, sizeof(dispatcher_msg_t));
    dispatcher_register_handler(TARGET_USB_MSC, io_usb_msc_dispatcher_handler);
    xTaskCreate(io_usb_msc_task, "io_usb_msc_task", 4096, NULL, 9, NULL);

    // Register MSC event callback for mount/unmount detection
    tinyusb_msc_set_storage_callback(msc_event_cb, NULL);

    // CDC dispatcher handler
    dispatcher_register_handler(TARGET_USB_CDC, io_usb_dispatcher_handler);

    ESP_LOGI(TAG, "USB Composite initialization DONE");
}

