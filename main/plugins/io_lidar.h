#ifndef IO_LIDAR_H
#define IO_LIDAR_H

#include "dispatcher.h"

// Initialize UART hardware + register dispatcher handler
void io_lidar_init(void);

// UART event task (internal to module, but declared here if ISR needs it)
void io_lidar_event_task(void *arg);

#endif // IO_LIDAR_H