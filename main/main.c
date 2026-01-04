#include "dispatcher.h"
#include "UMSeriesD_idf.h"
#include "io_usb.h"
#include "io_lidar.h"
#include "lidar_coordinator.h"
#include "io_rgb.h"
#include "io_battery.h"
#include "rgb_anim.h"
#include "rgb_anim_all.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void starting_color_test();

void app_main(void)
{

    ums3_begin();
    dispatcher_init();
    io_usb_init();
    io_rgb_init();
    io_lidar_init();
    lidar_coordinator_init();
    rgb_anim_init_all();
    io_battery_init();

    // Optionally, add a status log or LED blink here if desired.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}