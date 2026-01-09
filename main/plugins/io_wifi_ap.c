// io_wifi_ap.c - Wi-Fi AP plugin implementation
#include "io_wifi_ap.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

void io_wifi_ap_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = { 0 };
        strncpy((char *)wifi_config.ap.ssid, CONFIG_WIFI_AP_SSID, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen(CONFIG_WIFI_AP_SSID);
        wifi_config.ap.channel = CONFIG_WIFI_AP_CHANNEL;
        wifi_config.ap.max_connection = CONFIG_WIFI_AP_MAX_CONN;

            // Auth mode selection
        #if defined(CONFIG_WIFI_AUTH_OPEN)
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        #elif defined(CONFIG_WIFI_AUTH_WPA_PSK)
            wifi_config.ap.authmode = WIFI_AUTH_WPA_PSK;
        #elif defined(CONFIG_WIFI_AUTH_WPA2_PSK)
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        #elif defined(CONFIG_WIFI_AUTH_WPA_WPA2_PSK)
            wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        #else
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        #endif

            // Set password if required
            if (wifi_config.ap.authmode != WIFI_AUTH_OPEN) {
                strncpy((char *)wifi_config.ap.password, CONFIG_WIFI_AP_PASSWORD, sizeof(wifi_config.ap.password));
            }

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
            ESP_ERROR_CHECK(esp_wifi_start());

            ESP_LOGI("io_wifi_ap", "Wi-Fi AP started. SSID:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.channel);
        }
