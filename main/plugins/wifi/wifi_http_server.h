#ifndef WIFI_HTTP_SERVER_H
#define WIFI_HTTP_SERVER_H

#include <esp_http_server.h>

/**
 * @brief Initialize and start the HTTP server
 * Call after Wi-Fi AP is up
 */
void wifi_http_server_start(void);

#endif // WIFI_HTTP_SERVER_H
