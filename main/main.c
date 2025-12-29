#include "dispatcher.h"
#include "UMSeriesD_idf.h"
#include "io_usb.h"
#include "io_uart0.h"
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
    io_uart_init();
    io_rgb_init();
    rgb_anim_init_all();
    io_battery_init();
    starting_color_test();

    // Optionally, add a status log or LED blink here if desired.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void starting_color_test()
{
    dispatcher_msg_t msg = {0};

    msg.source = SOURCE_UNDEFINED;
    msg.target = TARGET_RGB;

    msg.data[0] = RGB_PLUGIN_SOLID;  // plugin_id
    msg.data[1] = 255;                // R
    msg.data[2] = 255;                // G
    msg.data[3] = 255;                // B
    msg.data[4] = 30;               // brightness (0–255)

    msg.message_len = 5;

    dispatcher_send(&msg);
}