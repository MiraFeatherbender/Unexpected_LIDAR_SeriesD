#ifndef IO_USB_MSC_H
#define IO_USB_MSC_H

#include "dispatcher.h"

// Initialize USB MSC hardware + register dispatcher handler
void io_usb_msc_init(void);

// Enable/disable MSC mode (expose FATFS to host)
void io_usb_msc_enable(void);
void io_usb_msc_disable(void);

// Optional: status query
bool io_usb_msc_is_enabled(void);

#endif // IO_USB_MSC_H
