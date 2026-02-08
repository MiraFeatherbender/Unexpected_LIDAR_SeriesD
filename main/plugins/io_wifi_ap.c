// io_wifi_ap.c - Wi-Fi AP plugin implementation
#include "io_wifi_ap.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_http_server.h"
#include "mdns.h"

#include "esp_netif.h"

#define MAX_STA_RETRIES 5
static int s_retry_num = 0;
static bool s_sta_connected = false;

static void start_ap_mode(void);

static void start_mdns(void) {
    static bool mdns_started = false;
    if (mdns_started) return;
    if (mdns_init() == ESP_OK) {
        mdns_hostname_set("AnkleBiter");
        mdns_instance_name_set("ESP32 Web Server");
        ESP_LOGI("io_wifi_ap", "mDNS started: http://AnkleBiter.local");
        mdns_started = true;
    } else {
        ESP_LOGW("io_wifi_ap", "mDNS init failed");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_STA_RETRIES) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("io_wifi_ap", "Retrying STA connection (%d/%d)", s_retry_num, MAX_STA_RETRIES);
        } else {
            ESP_LOGW("io_wifi_ap", "STA connection failed, switching to AP mode");
            start_ap_mode();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_sta_connected = true;
        s_retry_num = 0;
        ESP_LOGI("io_wifi_ap", "Connected in STA mode");
        start_mdns();
        wifi_http_server_start();
    }
}

static void start_sta_mode(void) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_STA_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_STA_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("io_wifi_ap", "Trying STA mode. SSID:%s", wifi_config.sta.ssid);
}

static void start_ap_mode(void) {
    esp_netif_create_default_wifi_ap();
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.ap.ssid, CONFIG_WIFI_AP_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(CONFIG_WIFI_AP_SSID);
    wifi_config.ap.channel = CONFIG_WIFI_AP_CHANNEL;
    wifi_config.ap.max_connection = CONFIG_WIFI_AP_MAX_CONN;

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

    if (wifi_config.ap.authmode != WIFI_AUTH_OPEN) {
        strncpy((char *)wifi_config.ap.password, CONFIG_WIFI_AP_PASSWORD, sizeof(wifi_config.ap.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("io_wifi_ap", "Wi-Fi AP started. SSID:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.channel);
    start_mdns();
    wifi_http_server_start();
}

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

    // configure static IP for STA and AP interfaces if needed
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton("192.168.12.85");
    ip_info.gw.addr = esp_ip4addr_aton("192.168.12.1");
    ip_info.netmask.addr = esp_ip4addr_aton("255.255.255.0");

    esp_netif_set_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

    s_retry_num = 0;
    s_sta_connected = false;
    start_sta_mode();
}
