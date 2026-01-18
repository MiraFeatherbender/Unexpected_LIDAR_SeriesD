
#include "noise_data.h"

#include <stdlib.h>

// Return a random integer in [min, max] inclusive
static inline int randrange(int min, int max) {
    return min + (rand() % (max - min + 1));
}

// Advance walk indices: static if min==max, else random in [min,max] (inclusive)
void noise_walk_step(uint8_t *x, uint8_t *y, const noise_walk_spec_t *spec) {
    int8_t dx = (spec->min_dx == spec->max_dx)
        ? spec->min_dx
        : (int8_t)randrange(spec->min_dx, spec->max_dx);
    int8_t dy = (spec->min_dy == spec->max_dy)
        ? spec->min_dy
        : (int8_t)randrange(spec->min_dy, spec->max_dy);
    *x = (uint8_t)((*x + dx + 256) % 256);
    *y = (uint8_t)((*y + dy + 256) % 256);
}
