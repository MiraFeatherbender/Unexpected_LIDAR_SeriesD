#include "dispatcher.h"
#include "UMSeriesD_idf.h"
#include "io_usb_cdc_msc.h"
#include "io_gpio.h"
#include "io_lidar.h"
#include "lidar_coordinator.h"
#include "io_rgb.h"
#include "io_battery.h"
#include "rgb_anim.h"
#include "rgb_anim_all.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plugins/io_wifi_ap.h"



void app_main(void)
{

    ums3_begin();
#ifdef CONFIG_UM_ANTENNA_EXTERNAL
    ums3_set_antenna_external(CONFIG_UM_ANTENNA_EXTERNAL);
#endif
    dispatcher_init();
    io_usb_cdc_msc_init();
    rgb_anim_init_all();
    io_rgb_init();
    io_gpio_init();
    io_lidar_init();
    lidar_coordinator_init();
    io_battery_init();

    // Start Wi-Fi AP
    io_wifi_ap_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
