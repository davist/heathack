
#ifndef __INC_PALETTES_H
#define __INC_PALETTES_H

#include "config.h"

const uint8_t NUM_PALETTES = 2;

struct Palette {
  uint8_t size;
  CRGB colours[8];
};

extern const Palette palettes[NUM_PALETTES];

enum PaletteNames { PALETTE_RAINBOW = 0, PALETTE_HALLOWEEN = 1 };

#endif

