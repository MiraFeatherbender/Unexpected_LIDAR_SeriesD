#ifndef NOISE_DATA_H
#define NOISE_DATA_H

#include <stdint.h>
// --- Walk Spec and Helpers ---
typedef struct {
    int8_t min_dx;
    int8_t max_dx;
    int8_t min_dy;
    int8_t max_dy;
} noise_walk_spec_t;

// Advance walk indices: static if min==max, else random in [min,max] (inclusive)
void noise_walk_step(uint8_t *x, uint8_t *y, const noise_walk_spec_t *spec);

#endif // NOISE_DATA_H
