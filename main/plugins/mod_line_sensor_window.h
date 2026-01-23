#ifndef MOD_LINE_SENSOR_WINDOW_H
#define MOD_LINE_SENSOR_WINDOW_H

#include "dispatcher.h"

void mod_line_sensor_window_init(void);

/* Set snapshot divisor: send every Nth snapshot (1 = every sample) */
void mod_line_sensor_window_set_snapshot_div(uint8_t div);

#endif // MOD_LINE_SENSOR_WINDOW_H