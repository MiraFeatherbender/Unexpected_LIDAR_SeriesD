idf_component_register(
    SRCS 
        "main.c"
        "dispatcher.c"
        "io_usb.c"
        "io_uart0.c"
        "io_rgb.c"
        "io_battery.c"
        "src/UMSeriesD_idf.c"
        "src/UMSeriesD.cpp"
    REQUIRES esp_driver_uart esp_driver_gpio esp_adc led_strip esp_driver_i2c
    INCLUDE_DIRS 
        "src"
)

# Enable floating point support in printf/scanf
target_link_libraries(${COMPONENT_LIB} "-u printf_float")