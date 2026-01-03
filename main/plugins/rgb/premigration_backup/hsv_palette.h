#ifndef HSV_PALETTE_H
#define HSV_PALETTE_H

#include <stdint.h>
// #include "UMSeriesD_idf.h" // for hsv_color_t
#include "rgb_anim.h"

#define HSV_PALETTE_SIZE 256

// HSV palette types
typedef enum {
    HSV_PALETTE_FIRE = 0,
    HSV_PALETTE_COUNT
} hsv_palette_t;

// Extern declarations for HSV palettes
extern const hsv_color_t hsv_palette_fire[HSV_PALETTE_SIZE];

// Helper to get pointer to selected palette
const hsv_color_t *get_hsv_palette(hsv_palette_t palette);

// Helper to shift hue with wrap-around
static inline uint8_t hsv_shift_hue(uint8_t h, uint8_t offset) {
    return (uint8_t)(h + offset);
}

#endif // HSV_PALETTE_H
