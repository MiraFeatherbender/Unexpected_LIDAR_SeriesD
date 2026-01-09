#include "wifi_http_server.h"
#include "io_fatfs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <esp_http_server.h>
#include <esp_log.h>

#define TAG "wifi_http_server"
#define INDEX_PATH "/data/index.html"

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

// Unified file handler for all GET requests
static esp_err_t file_get_handler(httpd_req_t *req) {
    char filepath[256];
    // Check for ?file=... query parameter
    char file_param[192] = {0};
    esp_err_t found = httpd_req_get_url_query_str(req, file_param, sizeof(file_param));
    char file_value[192] = {0};
    if (found == ESP_OK && httpd_query_key_value(file_param, "file", file_value, sizeof(file_value)) == ESP_OK) {
        snprintf(filepath, sizeof(filepath), "/data/%s", file_value);
    } else {
        // Serve index.html for root or fallback
        snprintf(filepath, sizeof(filepath), "/data/index.html");
    }
    ESP_LOGI(TAG, "Requested URI: %s", req->uri);
    ESP_LOGI(TAG, "Constructed filepath: %s", filepath);
    struct stat st;
    int stat_result = stat(filepath, &st);
    ESP_LOGI(TAG, "stat() result: %d", stat_result);
    if (stat_result != 0 || !S_ISREG(st.st_mode)) {
        ESP_LOGW(TAG, "File not found or not regular: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    static uint8_t buf[2048];
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGW(TAG, "fopen failed for: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, get_mime_type(filepath));
    int total_bytes = 0;
    while (1) {
        int bytes = fread(buf, 1, sizeof(buf), f);
        if (bytes > 0) {
            esp_err_t ret = httpd_resp_send_chunk(req, (const char *)buf, bytes);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "httpd_resp_send_chunk failed: %d", ret);
                fclose(f);
                httpd_resp_send_chunk(req, NULL, 0); // End response
                return ESP_FAIL;
            }
            total_bytes += bytes;
        }
        if (bytes < sizeof(buf)) {
            break; // EOF or error
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "Sent %d bytes from %s", total_bytes, filepath);
    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}

void wifi_http_server_start(void)
{
    if (server) return;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t catchall_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = file_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &catchall_uri);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
