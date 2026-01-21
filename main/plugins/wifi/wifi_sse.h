#ifndef WIFI_SSE_H
#define WIFI_SSE_H

#include "dispatcher.h"
#include "cJSON.h"
#include <esp_err.h>
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_sse_init(httpd_handle_t server);
esp_err_t wifi_sse_deinit(void);
void wifi_sse_broadcast(dispatch_target_t target, cJSON *payload);

/* HTTP handler exported for /sse (separate httpd instance) */
esp_err_t wifi_sse_event_source_handler(httpd_req_t *req);

/* Admin/test handler: POST /api/sse/push - forwards JSON -> sse_broadcast */
esp_err_t wifi_sse_push_handler(httpd_req_t *req);

/* Dispatcher callback to register with dispatcher for SSE-related targets */
void wifi_sse_dispatch_handler(const dispatcher_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif // WIFI_SSE_H
