#ifndef NOISE_DATA_H
#define NOISE_DATA_H

#include <stdint.h>

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

// Helper to get pointer to selected palette
const uint8_t (*get_noise_palette(noise_palette_t palette))[PGM_WIDTH];

#endif // NOISE_DATA_H
