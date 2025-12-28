#ifndef IO_RGB_H
#define IO_RGB_H

#include "dispatcher.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize RGB subsystem and register dispatcher handler
void io_rgb_init(void);

#ifdef __cplusplus
}
#endif

#endif // IO_RGB_H