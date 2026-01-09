#ifndef IO_USB_CDC_MSC_H
#define IO_USB_CDC_MSC_H

#include "dispatcher.h"

// Initialize USB CDC + MSC and register dispatcher handlers
void io_usb_cdc_msc_init(void);
// Check if MSC mode is currently enabled
bool io_usb_msc_is_enabled(void);

#endif // IO_USB_CDC_MSC_H
