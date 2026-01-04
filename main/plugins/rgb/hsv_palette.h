
#ifndef HSV_PALETTE_H
#define HSV_PALETTE_H

#include <stdint.h>
#include "rgb_anim.h"
#include "rgb_anim_composer.h"


#define HSV_PALETTE_SIZE 256

// HSV palette types
typedef enum {
    HSV_PALETTE_FIRE = 0,
    HSV_PALETTE_AURORA,
    HSV_PALETTE_WATER,
    // Add more palettes here as needed
    HSV_PALETTE_COUNT
} hsv_palette_t;

// Extern declarations for HSV palettes
extern const hsv_color_t hsv_palette_fire[HSV_PALETTE_SIZE];
extern const hsv_color_t hsv_palette_aurora[HSV_PALETTE_SIZE];
extern const hsv_color_t hsv_palette_water[HSV_PALETTE_SIZE];

// Helper to get pointer to selected palette (returns NULL if out of bounds)
const hsv_color_t *get_hsv_palette(hsv_palette_t palette);

// Lookup color in palette with HSV shift (8-bit wraparound per channel)
// Pass {0,0,0} for no shift
hsv_color_t hsv_palette_lookup(const hsv_color_t *palette, uint8_t index, hsv_color_t shift);

// --- Standard palette index strategies ---
uint8_t palette_index_multiply(uint8_t contrast, uint8_t gentle, uint8_t modifier); // (contrast * gentle) >> 8
uint8_t palette_index_average(uint8_t contrast, uint8_t gentle, uint8_t modifier); // (contrast + gentle) >> 1
uint8_t palette_index_blend(uint8_t contrast, uint8_t gentle, uint8_t modifier);   // (1-z)contrast + z*gentle, z=modifier / 2


// --- Standard brightness strategies ---
// See palette_brightness_ranking.md for rationale and usage
uint8_t brightness_index(const hsv_color_t in, uint8_t palette_index, uint8_t noise_gentle, uint8_t user_brightness); // Use palette index as brightness
uint8_t brightness_value(const hsv_color_t in, uint8_t unused1, uint8_t unused2, uint8_t user_brightness); // Use palette V channel
uint8_t brightness_value_noise(const hsv_color_t in, uint8_t unused1, uint8_t noise, uint8_t user_brightness); // V channel + gentle noise

#endif // HSV_PALETTE_H
