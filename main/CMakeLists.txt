# Automatically collect all animation source files
file(GLOB RGB_ANIMATION_SOURCES "plugins/rgb/*.c")
file(GLOB LIDAR_SOURCES "plugins/RPLIDAR/*.c")
#file(GLOB WIFI_SOURCES "plugins/wifi/*.c")

idf_component_register(
    SRCS
        "main.c"
        "dispatcher.c"

        # Core plugin sources
        "plugins/io_gpio.c"
        "plugins/io_usb_cdc_msc.c"
        "plugins/io_lidar.c"
        "plugins/io_rgb.c"
        "plugins/io_battery.c"
        "plugins/io_wifi_ap.c"
        "plugins/io_fatfs.c"

        # Animation sources (auto-discovered)
        ${RGB_ANIMATION_SOURCES}

        # LIDAR sources (auto-discovered)
        ${LIDAR_SOURCES}

        # WiFi sources
        "plugins/wifi/wifi_http_server.c"

        # Library sources
        "src/UMSeriesD_idf.c"

    REQUIRES
        esp_wifi
        esp_http_server
        esp_netif
        esp_event
        nvs_flash
        esp_driver_uart
        esp_driver_gpio
        esp_adc
        led_strip
        esp_driver_i2c
        fatfs
        wear_levelling
        esp_partition
        
    INCLUDE_DIRS
        "."               # main/
        "plugins"         # io_*.h
        "plugins/rgb"     # rgb_anim*.h
        "plugins/RPLIDAR" # lidar_*.h
        "plugins/wifi"    # wifi_*.h
        "src"             # UMSeriesD_idf.h
)

# Enable floating point support in printf/scanf
target_link_libraries(${COMPONENT_LIB} "-u printf_float")