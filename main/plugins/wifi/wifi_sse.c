#include "wifi_sse.h"
#include "wifi_http_server.h"
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "wifi_sse";
#define SSE_KEEPALIVE_MS 5000

typedef struct sse_client {
    httpd_req_t *req;
    uint64_t mask;
    struct sse_client *next;
} sse_client_t;

static httpd_handle_t sse_server = NULL;
static sse_client_t *clients = NULL;
static SemaphoreHandle_t clients_mutex = NULL;
static TaskHandle_t keepalive_task = NULL;

/* Map dispatch_target_t to a short event name (switch/case, enums used directly) */
static const char *target_to_event_name(dispatch_target_t t) {
    switch (t) {
        case TARGET_SSE_CONSOLE: return "console";
        case TARGET_SSE_LINE_SENSOR: return "line_sensor";
        case TARGET_SSE: return "sse";
        default: return NULL;
    }
}

/* Map textual CSV token to enum value. Called on connect only. */
static bool name_to_target(const char *name, dispatch_target_t *out) {
    if (!name || !out) return false;
    if (strcasecmp(name, "console") == 0) { *out = TARGET_SSE_CONSOLE; return true; }
    if (strcasecmp(name, "line_sensor") == 0) { *out = TARGET_SSE_LINE_SENSOR; return true; }
    if (strcasecmp(name, "sse") == 0) { *out = TARGET_SSE; return true; }
    return false;
}

static inline uint64_t mask_for_target(dispatch_target_t t) {
    unsigned u = (unsigned)t;
    if (u >= 64) return 0; // safety
    return 1ULL << u;
}

static uint64_t build_mask_from_csv(const char *csv) {
    if (!csv || csv[0] == '\0') return 0;
    char *copy = strdup(csv);
    if (!copy) return 0;
    uint64_t mask = 0;
    char *tok = strtok(copy, ",");
    while (tok) {
        dispatch_target_t t;
        if (name_to_target(tok, &t)) {
            mask |= mask_for_target(t);
        }
        tok = strtok(NULL, ",");
    }
    free(copy);
    return mask;
}

void wifi_sse_broadcast(dispatch_target_t target, cJSON *payload) {
    if (!payload) return;
    const char *ename = target_to_event_name(target);
    if (!ename) return;

    char *json = cJSON_PrintUnformatted(payload);
    if (!json) return;
    uint64_t bit = mask_for_target(target);

    if (!clients_mutex) {
        cJSON_free(json);
        return;
    }
    xSemaphoreTake(clients_mutex, portMAX_DELAY);
    sse_client_t **prev = &clients;
    while (*prev) {
        sse_client_t *c = *prev;
        if ((c->mask & bit) != 0) {
            char hdr[128];
            int n = snprintf(hdr, sizeof(hdr), "event: %s\n", ename);
            esp_err_t r = httpd_resp_send_chunk(c->req, hdr, n);
            if (r == ESP_OK) r = httpd_resp_send_chunk(c->req, "data: ", 6);
            if (r == ESP_OK) r = httpd_resp_send_chunk(c->req, json, strlen(json));
            if (r == ESP_OK) r = httpd_resp_send_chunk(c->req, "\n\n", 2);
            if (r != ESP_OK) {
                ESP_LOGW(TAG, "SSE client write failed, dropping client");
                *prev = c->next;
                httpd_resp_send_chunk(c->req, NULL, 0);
                httpd_req_async_handler_complete(c->req);
                free(c);
                continue; // prev unchanged
            }
        }
        prev = &(*prev)->next;
    }
    xSemaphoreGive(clients_mutex);
    cJSON_free(json);
}

void wifi_sse_dispatch_handler(const dispatcher_msg_t *msg) {
    if (!msg) return;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "source", (int)msg->source);
    cJSON_AddStringToObject(root, "level", "info");
    /* Use milliseconds since boot; browser Date(number) handles epoch ms-style values */
    cJSON_AddNumberToObject(root, "time", (double)(esp_timer_get_time() / 1000));
    if (msg->message_len > 0) {
        if (msg->source == SOURCE_LINE_SENSOR || msg->source == SOURCE_MSC_BUTTON) {
            size_t hex_len = msg->message_len;
            if (hex_len > 32) hex_len = 32; // limit payload size
            char hexbuf[32 * 3 + 1] = {0};
            size_t off = 0;
            for (size_t i = 0; i < hex_len; ++i) {
                off += snprintf(hexbuf + off, sizeof(hexbuf) - off, "%02X ", msg->data[i]);
                if (off >= sizeof(hexbuf)) break;
            }
            if (off > 0 && off < sizeof(hexbuf)) {
                hexbuf[off - 1] = '\0'; // trim trailing space
            }
            cJSON_AddStringToObject(root, "data", hexbuf);
            cJSON_AddStringToObject(root, "msg", hexbuf);
        } else {
            /* Add message bytes as a base64-safe string or plain text; for simplicity, treat as string */
            cJSON_AddStringToObject(root, "data", (const char*)msg->data);
            cJSON_AddStringToObject(root, "msg", (const char*)msg->data);
        }
    } else {
        cJSON_AddNullToObject(root, "data");
        cJSON_AddStringToObject(root, "msg", "");
    }

    /* For each target entry in the message, forward if it's an SSE target */
    for (int i = 0; i < TARGET_MAX; ++i) {
        dispatch_target_t t = msg->targets[i];
        if (t == TARGET_MAX) continue; // sentinel marking unused slot
        const char *ename = target_to_event_name(t);
        if (ename) {
            wifi_sse_broadcast(t, root);
        }
    }

    cJSON_Delete(root);
}

/* Utility to remove a client; assumes clients_lock held and prev points to previous->next */
static void remove_client_locked(sse_client_t **prev_next) {
    sse_client_t *c = *prev_next;
    if (!c) return;
    *prev_next = c->next;
    httpd_resp_send_chunk(c->req, NULL, 0);
    httpd_req_async_handler_complete(c->req);
    free(c);
}

esp_err_t wifi_sse_event_source_handler(httpd_req_t *req) {
    /* Parse query string for targets=csv using httpd helper */
    char csv[256] = {0};
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0) {
        /* cap query length to avoid excessive allocation */
        int cap = (qlen > 1024) ? 1024 : qlen;
        char *qbuf = malloc(cap + 1);
        if (qbuf) {
            if (httpd_req_get_url_query_str(req, qbuf, cap + 1) == ESP_OK) {
                const char *p = strstr(qbuf, "targets=");
                if (p) {
                    p += strlen("targets=");
                    strncpy(csv, p, sizeof(csv)-1);
                }
            }
            free(qbuf);
        }
    }
    uint64_t mask = build_mask_from_csv(csv);
    if (mask == 0) {
        /* default to TARGET_SSE background */
        mask = mask_for_target(TARGET_SSE);
    }

    httpd_req_t *async_req = NULL;
    esp_err_t async_rc = httpd_req_async_handler_begin(req, &async_req);
    if (async_rc != ESP_OK || !async_req) {
        ESP_LOGE(TAG, "Failed to begin async SSE handler");
        return ESP_FAIL;
    }

    httpd_resp_set_type(async_req, "text/event-stream");
    httpd_resp_set_hdr(async_req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(async_req, "Connection", "keep-alive");

    sse_client_t *c = calloc(1, sizeof(*c));
    if (!c) {
        httpd_req_async_handler_complete(async_req);
        return ESP_FAIL;
    }
    c->req = async_req;
    c->mask = mask;
    c->next = NULL;

    if (!clients_mutex) {
        httpd_req_async_handler_complete(async_req);
        free(c);
        return ESP_FAIL;
    }
    xSemaphoreTake(clients_mutex, portMAX_DELAY);
    c->next = clients;
    clients = c;
    xSemaphoreGive(clients_mutex);

    ESP_LOGI(TAG, "SSE client connected (mask=0x%016llx)", (unsigned long long)mask);

    return ESP_OK;
}

esp_err_t wifi_sse_push_handler(httpd_req_t *req) {
    if (req->content_len <= 0 || req->content_len > 8192) {
        httpd_resp_sendstr(req, "{\"status\":\"bad_request\"}");
        return ESP_FAIL;
    }
    char *buf = malloc(req->content_len + 1);
    if (!buf) return ESP_FAIL;
    int r = httpd_req_recv(req, buf, req->content_len);
    if (r <= 0) { free(buf); return ESP_FAIL; }
    buf[r] = '\0';
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *j_target = cJSON_GetObjectItem(root, "target");
    cJSON *j_payload = cJSON_GetObjectItem(root, "payload");
    if (!j_target || !j_payload) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Missing fields");
        return ESP_FAIL;
    }
    dispatch_target_t t;
    if (!name_to_target(j_target->valuestring, &t)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Unknown target");
        return ESP_FAIL;
    }

    wifi_sse_broadcast(t, j_payload);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static void wifi_sse_keepalive_task(void *arg) {
    const TickType_t keep_ticks = pdMS_TO_TICKS(SSE_KEEPALIVE_MS);
    while (1) {
        vTaskDelay(keep_ticks);
        if (!clients_mutex) continue;
        xSemaphoreTake(clients_mutex, portMAX_DELAY);
        sse_client_t **walk = &clients;
        while (*walk) {
            sse_client_t *c = *walk;
            esp_err_t r = httpd_resp_send_chunk(c->req, ": keepalive\n\n", strlen(": keepalive\n\n"));
            if (r != ESP_OK) {
                ESP_LOGI(TAG, "SSE client disconnected");
                remove_client_locked(walk);
                continue;
            }
            walk = &(*walk)->next;
        }
        xSemaphoreGive(clients_mutex);
    }
}

esp_err_t wifi_sse_init(httpd_handle_t server) {
    if (!server) return ESP_ERR_INVALID_ARG;
    if (sse_server) return ESP_OK;
    sse_server = server;
    if (!clients_mutex) {
        clients_mutex = xSemaphoreCreateMutex();
        if (!clients_mutex) {
            sse_server = NULL;
            return ESP_FAIL;
        }
    }

    if (!keepalive_task) {
        BaseType_t ok = xTaskCreate(wifi_sse_keepalive_task, "wifi_sse_keepalive", 3072, NULL, tskIDLE_PRIORITY+1, &keepalive_task);
        if (ok != pdTRUE) {
            keepalive_task = NULL;
            sse_server = NULL;
            return ESP_FAIL;
        }
    }

    httpd_uri_t uri = {
        .uri = "/sse",
        .method = HTTP_GET,
        .handler = wifi_sse_event_source_handler,
        .user_ctx = NULL
    };
    esp_err_t err = httpd_register_uri_handler(sse_server, &uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register SSE URI handler");
        sse_server = NULL;
        return err;
    }

    httpd_uri_t push_uri = {
        .uri = "/api/sse/push",
        .method = HTTP_POST,
        .handler = wifi_sse_push_handler,
        .user_ctx = NULL
    };
    err = httpd_register_uri_handler(sse_server, &push_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register SSE push URI handler");
        sse_server = NULL;
        return err;
    }

    /* Register dispatcher handlers for SSE targets */
    dispatcher_register_handler(TARGET_SSE_CONSOLE, wifi_sse_dispatch_handler);
    dispatcher_register_handler(TARGET_SSE_LINE_SENSOR, wifi_sse_dispatch_handler);
    dispatcher_register_handler(TARGET_SSE, wifi_sse_dispatch_handler);

    ESP_LOGI(TAG, "SSE handlers registered on shared HTTP server");
    return ESP_OK;
}

esp_err_t wifi_sse_deinit(void) {
    sse_server = NULL;
    if (keepalive_task) {
        vTaskDelete(keepalive_task);
        keepalive_task = NULL;
    }
    if (clients_mutex) {
        xSemaphoreTake(clients_mutex, portMAX_DELAY);
        sse_client_t *c = clients;
        while (c) {
            sse_client_t *n = c->next;
            httpd_resp_send_chunk(c->req, NULL, 0);
            httpd_req_async_handler_complete(c->req);
            free(c);
            c = n;
        }
        clients = NULL;
        xSemaphoreGive(clients_mutex);
    }
    return ESP_OK;
}
