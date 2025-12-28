#pragma once
#include <stdint.h>

class UMSeriesD {
public:
    // Initializes all UM Series[D] board peripherals
    void begin();

    // Set LDO2 on or off
    // Only available on the ProS3[D] and FeatherS3[D]
    void setLDO2Power(bool on);

    // Set RGB LED power on or off (On ProS3[D] and FeatherS3[D] it sets LDO2 on)
    // Not available on the EdgeS3[D]
    void setPixelPower(bool on);

    // Set RGB LED color
    // Not available on the EdgeS3[D]
    void setPixelColor(uint8_t r, uint8_t g, uint8_t b);
    void setPixelColor(uint32_t rgb);

    // Set RGB LED brightness
    // Not available on the EdgeS3[D]
    void setPixelBrightness(uint8_t brightness);

    // Pack r,g,b (0-255) into a 32bit rgb color
    static uint32_t color(uint8_t r, uint8_t g, uint8_t b);

    // Convert a color wheel angle (0-255) to a 32bit rgb color
    static uint32_t colorWheel(uint8_t pos);

    // Set the blue LED on or off
    // Only available on the FeatherS3[D]
    void setBlueLED(bool on);

    // Get the light sensor in volts (0-3.3)
    // Only available on the FeatherS3[D]
    float getLightSensorVoltage();

    // Toggle the blue LED
    // Only available on the FeatherS3[D]
    void toggleBlueLED();

    // Get the battery voltage in volts
    // This function gets the voltage from the MAX17048
    float getBatteryVoltage();

    // Detect if VBUS (USB power) is present
    // Not available on the EdgeS3[D]
    bool getVbusPresent();

    // Set the RF Switch to external antenna
    void setAntennaExternal(bool state);
    
    // Setup the fuel gauge (I2C for MAX17048)
    void fgSetup();

    // Get the MAX17048 fuel gauge version
    uint8_t fgVersion();
};
