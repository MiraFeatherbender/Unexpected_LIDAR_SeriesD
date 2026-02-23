#include "rgb_state_json.h"

#include "cJSON.h"
#include <string.h>

void rgb_state_json_fill(rest_json_request_t *req,
                         uint8_t plugin_id,
                         hsv_color_t hsv,
                         uint8_t brightness)
{
    if (!req || !req->json_buf || req->buf_size == 0) {
        return;
    }

    cJSON *root = cJSON_CreateObject();

    cJSON *plugin = cJSON_CreateObject();
    cJSON_AddNumberToObject(plugin, "id", plugin_id);
    if (plugin_id < RGB_PLUGIN_MAX) {
        cJSON_AddStringToObject(plugin, "name", rgb_plugin_names[plugin_id]);
    } else {
        cJSON_AddStringToObject(plugin, "name", "RGB_PLUGIN_UNKNOWN");
    }
    cJSON_AddItemToObject(root, "plugin", plugin);

    cJSON *plugins = cJSON_CreateArray();
    for (int i = 0; i < RGB_PLUGIN_MAX; ++i) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "id", i);
        cJSON_AddStringToObject(p, "name", rgb_plugin_names[i]);
        cJSON_AddItemToArray(plugins, p);
    }
    cJSON_AddItemToObject(root, "plugins", plugins);

    cJSON *parameters = cJSON_CreateArray();
    #define X_FIELD_HSV(name, ctype, jtype, desc) { \
        cJSON *f = cJSON_CreateObject(); \
        cJSON_AddStringToObject(f, "name", #name); \
        cJSON_AddNumberToObject(f, "value", hsv.name); \
        cJSON_AddStringToObject(f, "type", jtype); \
        cJSON_AddStringToObject(f, "desc", desc); \
        cJSON_AddItemToArray(parameters, f); \
    }
    #define X_FIELD_B(name, ctype, jtype, desc) { \
        cJSON *f = cJSON_CreateObject(); \
        cJSON_AddStringToObject(f, "name", #name); \
        cJSON_AddNumberToObject(f, "value", brightness); \
        cJSON_AddStringToObject(f, "type", jtype); \
        cJSON_AddStringToObject(f, "desc", desc); \
        cJSON_AddItemToArray(parameters, f); \
    }
    #include "io_rgb.def"
    #undef X_FIELD_B
    #undef X_FIELD_HSV
    cJSON_AddItemToObject(root, "parameters", parameters);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        size_t len = strlen(json_str);
        if (len < req->buf_size) {
            memcpy(req->json_buf, json_str, len + 1);
            if (req->json_len) {
                *req->json_len = len;
            }
        }
        cJSON_free(json_str);
    }

    cJSON_Delete(root);
}
