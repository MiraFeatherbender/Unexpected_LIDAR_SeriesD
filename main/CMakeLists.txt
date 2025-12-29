# Automatically collect all animation source files
file(GLOB RGB_ANIMATION_SOURCES "plugins/rgb/*.c")

idf_component_register(
    SRCS
        "main.c"
        "dispatcher.c"

        # Core plugin sources
        "plugins/io_usb.c"
        "plugins/io_uart0.c"
        "plugins/io_rgb.c"
        "plugins/io_battery.c"

        # Animation sources (auto-discovered)
        ${RGB_ANIMATION_SOURCES}

        # Library sources
        "src/UMSeriesD_idf.c"

    REQUIRES
        esp_driver_uart
        esp_driver_gpio
        esp_adc
        led_strip
        esp_driver_i2c

    INCLUDE_DIRS
        "."              # main/
        "plugins"        # io_*.h
        "plugins/rgb"    # rgb_anim*.h
        "src"            # UMSeriesD_idf.h
)

# Enable floating point support in printf/scanf
target_link_libraries(${COMPONENT_LIB} "-u printf_float")