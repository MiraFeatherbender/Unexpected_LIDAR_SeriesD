#ifndef NOISE_DATA_H
#define NOISE_DATA_H

#include <stdint.h>
#include <stdlib.h>

#define PGM_WIDTH 256
#define PGM_HEIGHT 256

// Noise palette types
typedef enum {
    NOISE_PALETTE_OPENSIMPLEX2 = 0,
    NOISE_PALETTE_PERLIN = 1,
    NOISE_PALETTE_COUNT
} noise_palette_t;

// Extern declarations for noise data arrays
extern const uint8_t openSimplex2_data[PGM_HEIGHT][PGM_WIDTH];
extern const uint8_t perlin_noise_data[PGM_HEIGHT][PGM_WIDTH];

// Helper to get pointer to selected palette (returns pointer to 2D array)
const uint8_t (*get_noise_palette(noise_palette_t palette))[PGM_WIDTH];

// Lookup value in noise field (x and y should be uint8_t for natural wraparound)
static inline uint8_t noise_palette_lookup(const uint8_t field[PGM_HEIGHT][PGM_WIDTH], uint8_t x, uint8_t y) {
    return field[y][x];
}

// --- Walk Spec and Helpers ---
typedef struct {
    int8_t min_dx;
    int8_t max_dx;
    int8_t min_dy;
    int8_t max_dy;
} noise_walk_spec_t;

typedef struct {
    const uint8_t (*data)[PGM_WIDTH];
    uint8_t x, y;
} noise_field_t;

// Advance walk indices: static if min==max, else random in [min,max] (inclusive)
void noise_walk_step(uint8_t *x, uint8_t *y, const noise_walk_spec_t *spec);

#endif // NOISE_DATA_H
