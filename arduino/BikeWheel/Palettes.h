
#ifndef __INC_PALETTES_H
#define __INC_PALETTES_H

#include "config.h"

#define NUM_PALETTES 2

struct Palette {
  uint8_t size;
  CRGB colours[8];
};

extern const Palette palettes[NUM_PALETTES];

#define PALETTE_HALLOWEEN 0
#define PALETTE_RAINBOW 1

#endif

