#ifndef IO_LOG_H
#define IO_LOG_H

#include "dispatcher.h"

// Initialize UART hardware + register dispatcher handler
void io_log_init(void);

// UART event task (internal to module, but declared here if ISR needs it)
void io_log_event_task(void *arg);

#endif // IO_LOG_H