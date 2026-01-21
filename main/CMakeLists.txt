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
        "plugins/io_log.c"

        # Animation sources (auto-discovered)
        ${RGB_ANIMATION_SOURCES}

        # LIDAR sources (auto-discovered)
        ${LIDAR_SOURCES}

        # WiFi sources
        "plugins/wifi/wifi_http_server.c"
        "plugins/wifi/wifi_sse.c"
        
        # Battery sources
        "plugins/battery/battery_json.c"

        # Library sources
        "src/UMSeriesD_idf.c"

        # Modules
        "plugins/mod_line_sensor_window.c"

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
        esp_timer

    INCLUDE_DIRS
        "."               # main/
        "plugins"         # io_*.h
        "plugins/rgb"     # rgb_anim*.h
        "plugins/RPLIDAR" # lidar_*.h
        "plugins/wifi"    # wifi_*.h
        "plugins/battery" # battery_json.h
        "src"             # UMSeriesD_idf.h
)

# Enable floating point support in printf/scanf
target_link_libraries(${COMPONENT_LIB} "-u printf_float")