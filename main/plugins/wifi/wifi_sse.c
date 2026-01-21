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
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "wifi_sse";

#define WIFI_SSE_PORT 9090
#define SSE_KEEPALIVE_MS 5000

typedef struct sse_client {
    httpd_req_t *req;
    uint64_t mask;
    struct sse_client *next;
} sse_client_t;

static httpd_handle_t sse_server = NULL;
static sse_client_t *clients = NULL;
static portMUX_TYPE clients_lock = portMUX_INITIALIZER_UNLOCKED;

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

    portENTER_CRITICAL(&clients_lock);
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
                free(c);
                continue; // prev unchanged
            }
        }
        prev = &(*prev)->next;
    }
    portEXIT_CRITICAL(&clients_lock);
    cJSON_free(json);
}

void wifi_sse_dispatch_handler(const dispatcher_msg_t *msg) {
    if (!msg) return;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "source", (int)msg->source);
    if (msg->message_len > 0) {
        /* Add message bytes as a base64-safe string or plain text; for simplicity, treat as string */
        cJSON_AddStringToObject(root, "data", (const char*)msg->data);
    } else {
        cJSON_AddNullToObject(root, "data");
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

    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    sse_client_t *c = calloc(1, sizeof(*c));
    if (!c) return ESP_FAIL;
    c->req = req;
    c->mask = mask;
    c->next = NULL;

    portENTER_CRITICAL(&clients_lock);
    c->next = clients;
    clients = c;
    portEXIT_CRITICAL(&clients_lock);

    ESP_LOGI(TAG, "SSE client connected (mask=0x%016llx)", (unsigned long long)mask);

    const TickType_t keep_ticks = pdMS_TO_TICKS(SSE_KEEPALIVE_MS);
    while (1) {
        vTaskDelay(keep_ticks);
        esp_err_t r = httpd_resp_send_chunk(req, ": keepalive\n\n", strlen(": keepalive\n\n"));
        if (r != ESP_OK) {
            /* remove client */
            portENTER_CRITICAL(&clients_lock);
            sse_client_t **walk = &clients;
            while (*walk) {
                if (*walk == c) {
                    remove_client_locked(walk);
                    break;
                }
                walk = &(*walk)->next;
            }
            portEXIT_CRITICAL(&clients_lock);
            ESP_LOGI(TAG, "SSE client disconnected");
            return ESP_OK;
        }
    }
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

static void wifi_sse_start_task(void *arg) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WIFI_SSE_PORT;
    /* pick a different control port to avoid colliding with the main httpd instance */
    /* default ctrl port is 32768; choose next port so multiple instances can coexist */
    config.ctrl_port = 32769;
    ESP_LOGI(TAG, "SSE httpd ctrl_port=%d", config.ctrl_port);

    /* reduce resource usage for separate instance */
    config.stack_size = 3072;
    config.max_uri_handlers = 4;
#ifdef HTTPD_MAX_OPEN_SOCKETS
    config.max_open_sockets = 2;
#endif
    config.uri_match_fn = httpd_uri_match_wildcard;

    UBaseType_t stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    const char *tname = pcTaskGetName(NULL);
    ESP_LOGI(TAG, "Attempting to start SSE server on port %d (free heap: %u bytes, task='%s', stack_hwm=%u)", WIFI_SSE_PORT, (unsigned)esp_get_free_heap_size(), tname ? tname : "", (unsigned)stack_hwm);
    esp_err_t rc = httpd_start(&sse_server, &config);
    if (rc != ESP_OK) {
        UBaseType_t stack_hwm2 = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGW(TAG, "Initial SSE server start failed: %s (%d). Free heap: %u bytes. task='%s' stack_hwm=%u. Retrying with minimal config.", esp_err_to_name(rc), rc, (unsigned)esp_get_free_heap_size(), tname ? tname : "", (unsigned)stack_hwm2);
        /* Retry with even smaller footprint */
        config.stack_size = 2048;
        config.max_uri_handlers = 2;
#ifdef HTTPD_MAX_OPEN_SOCKETS
        config.max_open_sockets = 1;
#endif
        vTaskDelay(pdMS_TO_TICKS(100));
        UBaseType_t stack_hwm3 = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Retrying SSE server start (free heap: %u bytes, task='%s', stack_hwm=%u)", (unsigned)esp_get_free_heap_size(), tname ? tname : "", (unsigned)stack_hwm3);
        rc = httpd_start(&sse_server, &config);
        if (rc != ESP_OK) {
            UBaseType_t stack_hwm4 = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGE(TAG, "SSE server failed to start after retry: %s (%d). Free heap: %u bytes. task='%s' stack_hwm=%u.", esp_err_to_name(rc), rc, (unsigned)esp_get_free_heap_size(), tname ? tname : "", (unsigned)stack_hwm4);
            sse_server = NULL;
            vTaskDelete(NULL);
            return;
        }
    }

    httpd_uri_t uri = {
        .uri = "/sse",
        .method = HTTP_GET,
        .handler = wifi_sse_event_source_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(sse_server, &uri);

    httpd_uri_t push_uri = {
        .uri = "/api/sse/push",
        .method = HTTP_POST,
        .handler = wifi_sse_push_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(sse_server, &push_uri);

    /* Register dispatcher handlers for SSE targets */
    dispatcher_register_handler(TARGET_SSE_CONSOLE, wifi_sse_dispatch_handler);
    dispatcher_register_handler(TARGET_SSE_LINE_SENSOR, wifi_sse_dispatch_handler);
    dispatcher_register_handler(TARGET_SSE, wifi_sse_dispatch_handler);

    ESP_LOGI(TAG, "SSE server started on port %d", WIFI_SSE_PORT);
    vTaskDelete(NULL);
}

esp_err_t wifi_sse_init(void) {
    if (sse_server) return ESP_OK;
    BaseType_t ok = xTaskCreate(wifi_sse_start_task, "wifi_sse_start", 8192, NULL, tskIDLE_PRIORITY+1, NULL);
    if (ok != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create SSE start task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t wifi_sse_deinit(void) {
    if (sse_server) {
        httpd_stop(sse_server);
        sse_server = NULL;
    }
    portENTER_CRITICAL(&clients_lock);
    sse_client_t *c = clients;
    while (c) {
        sse_client_t *n = c->next;
        httpd_resp_send_chunk(c->req, NULL, 0);
        free(c);
        c = n;
    }
    clients = NULL;
    portEXIT_CRITICAL(&clients_lock);
    return ESP_OK;
}
