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


// Handler for serving index.html at root
static uint8_t index_buf[2048];
static esp_err_t index_get_handler(httpd_req_t *req) {
    const char *filepath = "/data/index.html";
    struct stat st;
    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        ESP_LOGW(TAG, "index.html not found");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "index.html not found");
        return ESP_FAIL;
    }
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGW(TAG, "fopen failed for: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open index.html");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, get_mime_type(filepath));
    int total_bytes = 0;
    while (1) {
        int bytes = fread(index_buf, 1, sizeof(index_buf), f);
        if (bytes > 0) {
            esp_err_t ret = httpd_resp_send_chunk(req, (const char *)index_buf, bytes);
            if (ret != ESP_OK) {
                fclose(f);
                httpd_resp_send_chunk(req, NULL, 0);
                return ESP_FAIL;
            }
            total_bytes += bytes;
        }
        if (bytes < sizeof(index_buf)) break;
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "Sent %d bytes from %s", total_bytes, filepath);
    return ESP_OK;
}

// Handler for serving files under /images/*
static uint8_t image_buf[2048];
static esp_err_t image_get_handler(httpd_req_t *req) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/data%s", req->uri); // e.g., /data/images/foo.bmp
    struct stat st;
    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        ESP_LOGW(TAG, "Image file not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Image file not found");
        return ESP_FAIL;
    }
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGW(TAG, "fopen failed for: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open image file");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, get_mime_type(filepath));
    int total_bytes = 0;
    while (1) {
        int bytes = fread(image_buf, 1, sizeof(image_buf), f);
        if (bytes > 0) {
            esp_err_t ret = httpd_resp_send_chunk(req, (const char *)image_buf, bytes);
            if (ret != ESP_OK) {
                fclose(f);
                httpd_resp_send_chunk(req, NULL, 0);
                return ESP_FAIL;
            }
            total_bytes += bytes;
        }
        if (bytes < sizeof(image_buf)) break;
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "Sent %d bytes from %s", total_bytes, filepath);
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
        // Handler for root (index.html)
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        // Handler for images (wildcard)
        httpd_uri_t images_uri = {
            .uri = "/images/*",
            .method = HTTP_GET,
            .handler = image_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &images_uri);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
