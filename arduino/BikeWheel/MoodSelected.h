
#ifndef __INC_MOOD_SELECTED_H
#define __INC_MOOD_SELECTED_H

#include "MoodSequence.h"
#include "Palettes.h"
#include "config.h"


class MoodSelected : public MoodSequence {
  
  // palette of colours used in the anim
  CRGB animPalette[3] = {
      CRGB::Black,
      CRGB::White,
      0   // placeholder for current colour to be inserted
  };

  // names for the palette entries
  const static uint8_t PALETTE_BLACK = 0;
  const static uint8_t PALETTE_WHITE = 1;
  const static uint8_t PALETTE_CUR_COLOUR = 2;

  // sequence defining the animation
  SeqStep sequence[9] = {
      // step 0 - set all to black
      { ALL_LEDS, PALETTE_BLACK, 5, 60 },

      // steps 1 to 6 - turn on leds to required colour one by one
      { 0, PALETTE_CUR_COLOUR, 4, 10 },
      { 1, PALETTE_CUR_COLOUR, 4, 10 },
      { 2, PALETTE_CUR_COLOUR, 4, 10 },
      { 3, PALETTE_CUR_COLOUR, 4, 10 },
      { 4, PALETTE_CUR_COLOUR, 4, 10 },
      { 5, PALETTE_CUR_COLOUR, 4, 10 },

      // step 7 - flash all leds to white
      { ALL_LEDS, PALETTE_WHITE, 2, 150 },

      // step 8 - return to colour
      { ALL_LEDS, PALETTE_CUR_COLOUR, 5, 20 }
  };

public:
  MoodSelected(uint8_t index) : MoodSequence(sequence, animPalette, 9) {
    uint8_t paletteSize = palettes[SELECT_PALETTE].size;
    animPalette[PALETTE_CUR_COLOUR] = palettes[SELECT_PALETTE].colours[index % paletteSize];
  }

};

#endif

