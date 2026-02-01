#include "UMSeriesD_idf.h"
#include "dispatcher.h"
#include "dispatcher_allocator.h"
#include "dispatcher_pool.h"
#include "dispatcher_pool_test.h"
#include "io_usb_msc.h"
#include "io_ultrasonic.h"
#include "io_gpio.h"
#include "io_MCP23017.h"
#include "io_lidar.h"
#include "lidar_coordinator.h"
#include "io_rgb.h"
#include "io_battery.h"
#include "rgb_anim.h"
#include "rgb_anim_all.h"
#include "io_log.h"
#include "io_wifi_ap.h"
#include "mod_line_sensor_window.h"
#include "mcp23017_test.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"



void app_main(void)
{
    ums3_set_ldo2_power(false);
    ums3_begin();
    ums3_set_ldo2_power(true);
    ums3_set_pixel_brightness(0); // Turn off RGB initially
#ifdef CONFIG_UM_ANTENNA_EXTERNAL
    ums3_set_antenna_external(CONFIG_UM_ANTENNA_EXTERNAL);
#endif

    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("phy_init", ESP_LOG_WARN);
    esp_log_level_set("usb_msc", ESP_LOG_WARN);
    esp_log_level_set("pp", ESP_LOG_WARN);
    esp_log_level_set("net80211", ESP_LOG_WARN);
    esp_log_level_set("mdns_mem", ESP_LOG_WARN);


    io_usb_msc_init();
    dispatcher_allocator_init();
    dispatcher_pool_init();
    dispatcher_init();
    io_log_init();
    io_gpio_init();
    io_ultrasonic_init();
    mod_line_sensor_window_init();
    io_lidar_init();
    lidar_coordinator_init();
    io_wifi_ap_init();
    io_battery_init();
    io_MCP23017_init();
    // mcp23017_test_start();

    rgb_anim_init_all();
    io_rgb_init();



    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
