#ifndef IO_ULTRASONIC_H
#define IO_ULTRASONIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the io_ultrasonic module and start its dispatcher task
void io_ultrasonic_init(void);

#ifdef __cplusplus
}
#endif

#endif // IO_ULTRASONIC_H
