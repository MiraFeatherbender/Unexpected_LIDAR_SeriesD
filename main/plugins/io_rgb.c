#include "io_rgb.h"
#include "dispatcher.h"
#include "UMSeriesD_idf.h"
#include "rgb_anim.h"
#include "rest_context.h"
#include "cJSON.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define RGB_CMD_QUEUE_LEN 32
#define RGB_TASK_STACK_SIZE 4096
#define RGB_TASK_PRIORITY 5

// Queue for incoming RGB commands
static QueueHandle_t rgb_cmd_queue = NULL;

// Plugin registry (dispatcher‑style)
static const rgb_anim_t *rgb_plugins[RGB_PLUGIN_MAX] = {0};

// Active plugin + current parameters
static const rgb_anim_t *active_anim = NULL;
static hsv_color_t current_hsv = {0, 0, 0};
static uint8_t current_brightness = 255;

// Track last command to avoid resetting animation unnecessarily
static uint8_t last_plugin_id = RGB_PLUGIN_MAX; // invalid sentinel
static hsv_color_t last_hsv = {0, 0, 0};
static uint8_t last_brightness = 255;
static bool has_last = false;

// Helper: set animation/plugin and parameters
static void rgb_set_animation(uint8_t plugin_id, uint8_t h, uint8_t s, uint8_t v, uint8_t brightness) {
    hsv_color_t new_hsv = {h, s, v};
    uint8_t new_brightness = brightness;

    bool plugin_changed = !has_last || (plugin_id != last_plugin_id);
    bool params_changed =
        !has_last ||
        (new_hsv.h != last_hsv.h) ||
        (new_hsv.s != last_hsv.s) ||
        (new_hsv.v != last_hsv.v) ||
        (new_brightness != last_brightness);

    // Select plugin
    active_anim = rgb_plugins[plugin_id];

    if (active_anim) {
        // Only reset animation when plugin changes
        if (plugin_changed && active_anim->begin)
            active_anim->begin();

        // Always push updated parameters when they change
        if (params_changed) {
            if (active_anim->set_color)
                active_anim->set_color(new_hsv);

            if (active_anim->set_brightness)
                active_anim->set_brightness(new_brightness);
        }
    }

    // Commit new state
    current_hsv = new_hsv;
    current_brightness = new_brightness;
    last_plugin_id = plugin_id;
    last_hsv = new_hsv;
    last_brightness = new_brightness;
    has_last = true;
}

// HSV8 to RGB888 conversion (0-255 range)
enum { SRC_V = 0, SRC_P = 1, SRC_Q = 2, SRC_T = 3 };

static const uint8_t rgb_src[6][3] = {
    //  R       G       B
    { SRC_V, SRC_T, SRC_P }, // region 0
    { SRC_Q, SRC_V, SRC_P }, // region 1
    { SRC_P, SRC_V, SRC_T }, // region 2
    { SRC_P, SRC_Q, SRC_V }, // region 3
    { SRC_T, SRC_P, SRC_V }, // region 4
    { SRC_V, SRC_P, SRC_Q }, // region 5
};

static uint8_t anim_brightness = 255;

void io_rgb_set_anim_brightness(uint8_t b)
{
    anim_brightness = b;
}


void hsv8_to_rgb888(uint8_t h, uint8_t s, uint8_t v,
                      uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        // Achromatic (grey)
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    uint16_t h6 = (uint16_t)h * 6; // Scale to 0-1530
    uint8_t region = h6 >> 8; // 0-5
    uint8_t remainder = h6 & 0xFF; // 0-255

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    const uint8_t *src = rgb_src[region];
    uint8_t vals[4] = { v, p, q, t };

    *r = vals[src[0]];
    *g = vals[src[1]];
    *b = vals[src[2]];
}

// Registration API (mirrors dispatcher_register_handler)
void io_rgb_register_plugin(rgb_plugin_id_t id, const rgb_anim_t *plugin)
{
    if (id < RGB_PLUGIN_MAX)
        rgb_plugins[id] = plugin;
}

// Forward declaration
static void io_rgb_task(void *arg);

// Dispatcher handler — messages routed TO RGB
static void io_rgb_dispatcher_handler(const dispatcher_msg_t *msg)
{
    // ESP_LOGI("io_rgb", "xQueueSend: source=%d, context=%p", msg->source, msg->context);
    if (xQueueSend(rgb_cmd_queue, msg, 0) != pdTRUE) {
        ESP_LOGW("io_rgb", "xQueueSend FAILED: source=%d, context=%p", msg->source, msg->context);
        // If queue is full and this is a REST GET, signal failure immediately
        if (msg->source == SOURCE_REST && msg->context) {
            rest_json_request_t *req = (rest_json_request_t *)msg->context;
            ESP_LOGW("io_rgb", "Giving semaphore immediately due to queue full (REST GET)");
            if (req->sem) xSemaphoreGive(req->sem);
        }
    }
}

void io_rgb_init(void)
{
    // Create command queue
    rgb_cmd_queue = xQueueCreate(RGB_CMD_QUEUE_LEN, sizeof(dispatcher_msg_t));

    // Register with dispatcher
    dispatcher_register_handler(TARGET_RGB, io_rgb_dispatcher_handler);

    // Initialize RGB hardware
    ums3_set_pixel_brightness(current_brightness);
    ums3_set_pixel_color(0, 0, 0);

    // Start RGB task
    xTaskCreate(io_rgb_task,
                "io_rgb_task",
                RGB_TASK_STACK_SIZE,
                NULL,
                RGB_TASK_PRIORITY,
                NULL);
}



static uint64_t priority = 0; // Bitfield to track message source priority

// Helper: Generate self-describing RGB JSON for REST GET
static void io_rgb_generate_json(rest_json_request_t *req) {
    cJSON *root = cJSON_CreateObject();

    // Plugin info
    cJSON *plugin = cJSON_CreateObject();
    cJSON_AddNumberToObject(plugin, "id", last_plugin_id);
    cJSON_AddStringToObject(plugin, "name", rgb_plugin_names[last_plugin_id]);
    cJSON_AddItemToObject(root, "plugin", plugin);

    // Plugins array
    cJSON *plugins = cJSON_CreateArray();
    for (int i = 0; i < RGB_PLUGIN_MAX; ++i) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "id", i);
        cJSON_AddStringToObject(p, "name", rgb_plugin_names[i]);
        cJSON_AddItemToArray(plugins, p);
    }
    cJSON_AddItemToObject(root, "plugins", plugins);

    // parameters array (using X-Macro)
    cJSON *parameters = cJSON_CreateArray();
    #define X_FIELD_HSV(name, ctype, jtype, desc) { \
        cJSON *f = cJSON_CreateObject(); \
        cJSON_AddStringToObject(f, "name", #name); \
        cJSON_AddNumberToObject(f, "value", current_hsv.name); \
        cJSON_AddStringToObject(f, "type", jtype); \
        cJSON_AddStringToObject(f, "desc", desc); \
        cJSON_AddItemToArray(parameters, f); \
    }
    #define X_FIELD_B(name, ctype, jtype, desc) { \
        cJSON *f = cJSON_CreateObject(); \
        cJSON_AddStringToObject(f, "name", #name); \
        cJSON_AddNumberToObject(f, "value", current_brightness); \
        cJSON_AddStringToObject(f, "type", jtype); \
        cJSON_AddStringToObject(f, "desc", desc); \
        cJSON_AddItemToArray(parameters, f); \
    }
    #include "io_rgb.def"
    #undef X_FIELD_B
    #undef X_FIELD_HSV
    cJSON_AddItemToObject(root, "parameters", parameters);

    // Print to buffer
    char *json_str = cJSON_PrintUnformatted(root);
    size_t len = strlen(json_str);
    if (len < req->buf_size) {
        memcpy(req->json_buf, json_str, len + 1);
        if (req->json_len) *req->json_len = len;
    }
    cJSON_free(json_str);
    cJSON_Delete(root);
}

static void io_rgb_task(void *arg)
{
    dispatcher_msg_t msg = {0};
    TickType_t rest_off_until = 0;

    while (1) {

        // Clear REST priority if timeout elapsed
        if (rest_off_until != 0 && xTaskGetTickCount() >= rest_off_until) {
            // Clear REST priority after timeout
            priority &= ~(1ULL << SOURCE_REST);
            rest_off_until = 0;
        }

        if (xQueueReceive(rgb_cmd_queue, &msg, 0) == pdTRUE) {
            priority |= (1ULL << msg.source);

            if ((priority >> (msg.source + 1)) != 0) {
                // Higher priority source active, ignore this message
                continue;
            }

            switch (msg.source) {
                case SOURCE_REST:
                    if (msg.context) {
                        rest_json_request_t *req = (rest_json_request_t *)msg.context;
                        ESP_LOGI("io_rgb", "SOURCE_REST: GET context path entered, context=%p, sem=%p", msg.context, req->sem);
                        io_rgb_generate_json(req);
                        xSemaphoreGive(req->sem);
                    }
                    else {
                        ESP_LOGI("io_rgb", "SOURCE_REST: COMMAND path entered, context=NULL, msg.data[0]=%d, msg.data[1]=%d, msg.data[2]=%d, msg.data[3]=%d, msg.data[4]=%d, msg_len=%d", msg.data[0], msg.data[1], msg.data[2], msg.data[3], msg.data[4], msg.message_len);
                        // REST command to change RGB
                        if (msg.message_len >= 5) {
                            uint8_t plugin_id = msg.data[0];
                            uint8_t h = msg.data[1];
                            uint8_t s = msg.data[2];
                            uint8_t v = msg.data[3];
                            uint8_t brightness = msg.data[4];
                            rgb_set_animation(plugin_id, h, s, v, brightness);
                        }
                        if(msg.data[0] == RGB_PLUGIN_OFF) {
                            // Clear priority for REST if turning off
                            // priority &= ~(1ULL << SOURCE_REST);
                            rest_off_until = xTaskGetTickCount() + pdMS_TO_TICKS(5000); // 5 seconds off
                        }
                    }
                    break;
                case SOURCE_USB_MSC:
                case SOURCE_MSC_BUTTON: 
                    if (msg.message_len >= 1) {
                        if (msg.data[0] == 0xA5) {
                            // Assert override for both sources
                            priority |= (1ULL << SOURCE_USB_MSC) | (1ULL << SOURCE_MSC_BUTTON);
                            rgb_set_animation(RGB_PLUGIN_HEARTBEAT, 190, 220, 140, 64);
                        } else if (msg.data[0] == 0x5A) {
                            // Release override for both sources
                            priority &= ~((1ULL << SOURCE_USB_MSC) | (1ULL << SOURCE_MSC_BUTTON));
                        }
                    }
                    break;

                default:
                    if (msg.message_len >= 5) {
                        uint8_t plugin_id = msg.data[0];
                        uint8_t h = msg.data[1];
                        uint8_t s = msg.data[2];
                        uint8_t v = msg.data[3];
                        uint8_t brightness = msg.data[4];
                        rgb_set_animation(plugin_id, h, s, v, brightness);
                    }
                    break;
            }
        }

        // Run active animation and get HSV output
        hsv_color_t out_hsv = current_hsv;
        if (active_anim && active_anim->step)
            active_anim->step(&out_hsv);

        // Convert HSV to RGB and output
        uint8_t r, g, b;
        hsv8_to_rgb888(out_hsv.h, out_hsv.s, out_hsv.v, &r, &g, &b);
        ums3_set_pixel_brightness(anim_brightness);
        ums3_set_pixel_color(r, g, b);

        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}