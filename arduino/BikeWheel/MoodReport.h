
#ifndef __INC_MOOD_REPORT_H
#define __INC_MOOD_REPORT_H

#include "MoodSequence.h"
#include "Palettes.h"
#include "config.h"


class MoodReport : public MoodSequence {

  // palette of colours used in the anim
  CRGB animPalette[8] = {
      CRGB::Black,
      CRGB::White,
      // placeholders for current colour to be inserted
      0,
      1,
      2,
      3,
      4,
      5
  };

  // names for the palette entries
  const static uint8_t PALETTE_BLACK = 0;
  const static uint8_t PALETTE_WHITE = 1;
  const static uint8_t PALETTE_RANKING_BASE = 2;

  // sequence defining the animation
  SeqStep sequence[13] = {
      // step 0 - set all to black
      { ALL_LEDS, PALETTE_BLACK, 5, 60 },

      // flash each led to white and then fade to ranking colour
      { 0, PALETTE_WHITE, 2, 150 },
      { 0, PALETTE_RANKING_BASE, 10, 20 },

      { 1, PALETTE_WHITE, 2, 150 },
      { 1, PALETTE_RANKING_BASE+1, 20, 20 },

      { 2, PALETTE_WHITE, 2, 150 },
      { 2, PALETTE_RANKING_BASE+2, 20, 20 },

      { 3, PALETTE_WHITE, 2, 150 },
      { 3, PALETTE_RANKING_BASE+3, 20, 20 },

      { 4, PALETTE_WHITE, 2, 150 },
      { 4, PALETTE_RANKING_BASE+4, 20, 20 },

      { 5, PALETTE_WHITE, 2, 150 },
      { 5, PALETTE_RANKING_BASE+5, 150, 20 },

  };

public:
  /**
   * rankings gives the order to display the colours
   */
  MoodReport(uint8_t *rankings) : MoodSequence(sequence, animPalette, 13) {
    uint8_t paletteSize = palettes[SELECT_PALETTE].size;

    // copy colours from palette to animPalette based on the rankings
    for (uint8_t l=0; l<NUM_LEDS; l++) {
      if (rankings[l] == 255) {
        animPalette[PALETTE_RANKING_BASE + l] = CRGB::Black;
      }
      else {
        animPalette[PALETTE_RANKING_BASE + l] = palettes[REPORT_PALETTE].colours[rankings[l] % paletteSize];
      }
    }
  }

};

#endif

