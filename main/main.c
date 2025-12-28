#include "dispatcher.h"
#include "UMSeriesD_idf.h"
#include "io_usb.h"
#include "io_uart0.h"
#include "io_rgb.h"
#include "io_battery.h"
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
    io_battery_init();
    starting_color_test();

    // Optionally, add a status log or LED blink here if desired.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void starting_color_test()
{
    dispatcher_msg_t msg;
    msg.source = SOURCE_UNDEFINED;
    msg.target = TARGET_RGB;
    msg.message_len = 4;
    msg.data[0] = 0x01;  // Command: set solid color
    msg.data[1] = 50;   // Red
    msg.data[2] = 50;   // Green
    msg.data[3] = 50;   // Blue

    dispatcher_send(&msg);
}