#ifndef IO_UART0_H
#define IO_UART0_H

#include "dispatcher.h"

// Initialize UART hardware + register dispatcher handler
void io_uart_init(void);

// UART event task (internal to module, but declared here if ISR needs it)
void io_uart_event_task(void *arg);

#endif // IO_UART0_H