#include "wifi_http_server.h"
#include "io_fatfs.h"
#include "dispatcher.h"
#include "rest_context.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <esp_http_server.h>
#include <esp_log.h>

#define TAG "wifi_http_server"
#define INDEX_PATH "/data/index.html"
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static httpd_handle_t server = NULL;

static const char *get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".bmp") == 0) return "image/bmp";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    // Add more types as needed
    return "application/octet-stream";
}



// --- Centralized buffer for file operations ---
static uint8_t file_op_buf[2048];

// --- Centralized error response helper ---
static const char *status_str(int status) {
    switch (status) {
        case HTTPD_400_BAD_REQUEST: return "400 Bad Request";
        case HTTPD_404_NOT_FOUND: return "404 Not Found";
        case HTTPD_500_INTERNAL_SERVER_ERROR: return "500 Internal Server Error";
        default: return "500 Internal Server Error";
    }
}

static esp_err_t send_http_error(httpd_req_t *req, int status, const char *msg) {
    httpd_resp_set_status(req, status_str(status));
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
}

// --- Centralized file serving logic ---
static esp_err_t serve_file(httpd_req_t *req, const char *filepath, const char *content_type) {
    struct stat st;
    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        ESP_LOGW(TAG, "File not found: %s", filepath);
        send_http_error(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGW(TAG, "fopen failed for: %s", filepath);
        send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, content_type ? content_type : get_mime_type(filepath));
    int total_bytes = 0;
    while (1) {
        int bytes = fread(file_op_buf, 1, sizeof(file_op_buf), f);
        if (bytes > 0) {
            esp_err_t ret = httpd_resp_send_chunk(req, (const char *)file_op_buf, bytes);
            if (ret != ESP_OK) {
                fclose(f);
                httpd_resp_send_chunk(req, NULL, 0);
                return ESP_FAIL;
            }
            total_bytes += bytes;
        }
        if (bytes < sizeof(file_op_buf)) break;
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "Sent %d bytes from %s", total_bytes, filepath);
    return ESP_OK;
}

// --- Centralized upload logic ---
static esp_err_t handle_upload(httpd_req_t *req, const char *filename) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/data/%s", filename);
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGW(TAG, "Failed to open for write: %s", filepath);
        send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file for writing");
        return ESP_FAIL;
    }
    int remaining = req->content_len;
    int total_bytes = 0;
    while (remaining > 0) {
        int to_read = remaining > sizeof(file_op_buf) ? sizeof(file_op_buf) : remaining;
        int read = httpd_req_recv(req, (char *)file_op_buf, to_read);
        if (read <= 0) {
            fclose(f);
            send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file data");
            return ESP_FAIL;
        }
        int written = fwrite(file_op_buf, 1, read, f);
        if (written != read) {
            fclose(f);
            send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file data");
            return ESP_FAIL;
        }
        total_bytes += written;
        remaining -= read;
    }
    fclose(f);
    ESP_LOGI(TAG, "Uploaded %d bytes to %s", total_bytes, filepath);
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

// --- Modular handler wrappers ---
static esp_err_t index_get_handler(httpd_req_t *req) {
    return serve_file(req, "/data/index.html", "text/html");
}

static esp_err_t image_get_handler(httpd_req_t *req) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/data%s", req->uri); // e.g., /data/images/foo.bmp
    return serve_file(req, filepath, NULL);
}

static esp_err_t upload_post_handler(httpd_req_t *req) {
    // Extract filename from URI after /upload/
    const char *uri = req->uri;
    const char *filename = uri + strlen("/upload/");
    if (strlen(filename) == 0 || strlen(filename) > 200) {
        send_http_error(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }
    return handle_upload(req, filename);
}


// --- Unified dispatcher helper for REST handlers ---
static esp_err_t dispatch_from_rest(const httpd_req_t *req, void *user_ctx, const void *data, size_t len) {
    if (!user_ctx || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    dispatcher_msg_t msg = {0};
    msg.source = SOURCE_REST;
    msg.targets[0] = (dispatch_target_t)(intptr_t)user_ctx; // user_ctx is (void*)(TARGET_*)
    msg.message_len = len > sizeof(msg.data) ? sizeof(msg.data) : len;
    memcpy(msg.data, data, msg.message_len);
    dispatcher_send(&msg);
    return ESP_OK;
}

static esp_err_t json_get_handler(httpd_req_t *req) {
    char buf[1024];
    rest_json_request_t rest_ctx = {
        .json_buf = buf,
        .buf_size = sizeof(buf),
        .json_len = NULL,
        .sem = xSemaphoreCreateBinary(),
        .user_data = NULL
    };
    size_t json_len = 0;
    rest_ctx.json_len = &json_len;

    // Use the endpoint's user_ctx as the target
    void *target = req->user_ctx;
    if (!target) {
        send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No target for JSON GET");
        vSemaphoreDelete(rest_ctx.sem);
        return ESP_FAIL;
    }

    esp_err_t err = dispatch_from_rest(req, target, &rest_ctx, sizeof(rest_ctx));
    if (err != ESP_OK) {
        send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Dispatch failed");
        vSemaphoreDelete(rest_ctx.sem);
        return ESP_FAIL;
    }

    // Wait for the JSON to be generated (timeout: 1s)
    if (xSemaphoreTake(rest_ctx.sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Timeout waiting for JSON");
        vSemaphoreDelete(rest_ctx.sem);
        return ESP_FAIL;
    }

    vSemaphoreDelete(rest_ctx.sem);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, rest_ctx.json_buf, json_len);
    return ESP_OK;
}

static esp_err_t rgb_handler(httpd_req_t *req) {
    switch(req->method) {
        case HTTP_GET:
            break;
        case HTTP_POST: {
            char buf[128];
            int ret = httpd_req_recv(req, buf, min(req->content_len, sizeof(buf)-1));
            if (ret <= 0) {
                send_http_error(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
                return ESP_FAIL;
            }
            buf[ret] = '\0';

            cJSON *json = cJSON_Parse(buf);
            if (!json) {
                send_http_error(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
                return ESP_FAIL;
            }

            uint8_t data[5];
            data[0] = (uint8_t)cJSON_GetObjectItem(json, "plugin")->valueint;
            data[1] = (uint8_t)cJSON_GetObjectItem(json, "h")->valueint;
            data[2] = (uint8_t)cJSON_GetObjectItem(json, "s")->valueint;
            data[3] = (uint8_t)cJSON_GetObjectItem(json, "v")->valueint;
            data[4] = (uint8_t)cJSON_GetObjectItem(json, "b")->valueint;
            cJSON_Delete(json);

            esp_err_t err = dispatch_from_rest(req, (void*)(intptr_t)TARGET_RGB, data, sizeof(data));
            if (err != ESP_OK) {
                send_http_error(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Dispatch failed");
                return ESP_FAIL;
            }
            httpd_resp_sendstr(req, "OK");
            return ESP_OK;
        }
        default:
            send_http_error(req, HTTPD_400_BAD_REQUEST, "Unsupported method");
            return ESP_FAIL;
    }

    // Dummy implementation for RGB control
    ESP_LOGI(TAG, "RGB handler invoked");
    httpd_resp_sendstr(req, "RGB control not implemented yet");
    return ESP_OK;
}



typedef esp_err_t (*http_handler_fn_t)(httpd_req_t *req);
typedef struct {
    const char *uri;
    httpd_method_t method;
    http_handler_fn_t handler;
    void *user_ctx;
} http_server_route_t;

#define X_REST_ENDPOINT(uri, method, handler, ctx) \
    { uri, method, handler, (void*)(intptr_t)ctx },
static const http_server_route_t http_server_routes[] = {
    { "/", HTTP_GET, index_get_handler, NULL },
    { "/images/*", HTTP_GET, image_get_handler, NULL },
    { "/upload/*", HTTP_POST, upload_post_handler, NULL },
    #include "rest_endpoints.def"
#undef X_REST_ENDPOINT
};

static esp_err_t register_http_server_routes(httpd_handle_t server) {
    for (size_t i = 0; i < sizeof(http_server_routes)/sizeof(http_server_routes[0]); ++i) {
        httpd_uri_t uri = {
            .uri      = http_server_routes[i].uri,
            .method   = http_server_routes[i].method,
            .handler  = http_server_routes[i].handler,
            .user_ctx = http_server_routes[i].user_ctx,
        };
        esp_err_t err = httpd_register_uri_handler(server, &uri);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI handler for %s", uri.uri);
            return err;
        }
    }
    return ESP_OK;
}

void wifi_http_server_start(void)
{
    if (server) return;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard; // Enable wildcard matching
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        register_http_server_routes(server);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
