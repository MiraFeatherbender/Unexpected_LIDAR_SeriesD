#ifndef IO_USB_CDC_H
#define IO_USB_CDC_H

#include "dispatcher.h"
#include "tinyusb_cdc_acm.h"

// Initialize USB hardware + register dispatcher handler
void io_usb_cdc_init(void);

// TinyUSB RX callback (must remain global)
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event);

#endif // IO_USB_CDC_H