idf_component_register(
    SRCS 
        "main-bridge.c"
        "dispatcher.c"
        "io_usb.c"
        "io_uart0.c"
        "io_rgb.c"
        "src/UMSeriesD_idf.c"
        "src/UMSeriesD.cpp"
    REQUIRES esp_driver_uart esp_driver_gpio esp_adc led_strip esp_driver_i2c
    INCLUDE_DIRS 
        "src"
)
