#include "UMSeriesD.hpp"
#include "UMSeriesD_idf.h"

void UMSeriesD::begin() { ums3_begin(); }
void UMSeriesD::setLDO2Power(bool on) { ums3_set_ldo2_power(on); }
void UMSeriesD::setPixelPower(bool on) { ums3_set_pixel_power(on); }
void UMSeriesD::setPixelColor(uint8_t r, uint8_t g, uint8_t b) { ums3_set_pixel_color(r, g, b); }
void UMSeriesD::setPixelColor(uint32_t rgb) { ums3_set_pixel_color32(rgb); }
void UMSeriesD::setPixelBrightness(uint8_t brightness) { ums3_set_pixel_brightness(brightness); }
uint32_t UMSeriesD::color(uint8_t r, uint8_t g, uint8_t b) { return ums3_color(r, g, b); }
uint32_t UMSeriesD::colorWheel(uint8_t pos) { return ums3_color_wheel(pos); }
void UMSeriesD::setBlueLED(bool on) { ums3_set_blue_led(on); }
float UMSeriesD::getLightSensorVoltage() { return ums3_get_light_sensor_voltage(); }
void UMSeriesD::toggleBlueLED() { ums3_toggle_blue_led(); }
float UMSeriesD::getBatteryVoltage() { return ums3_get_battery_voltage(); }
bool UMSeriesD::getVbusPresent() { return ums3_get_vbus_present(); }
void UMSeriesD::setAntennaExternal(bool state) { ums3_set_antenna_external(state); }
void UMSeriesD::fgSetup() { ums3_fg_setup(); }
uint8_t UMSeriesD::fgVersion() { return ums3_fg_version(); }