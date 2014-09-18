#ifndef __INC_PATTERN_SNOW_H
#define __INC_PATTERN_SNOW_H

#include "Pattern.h"
#include "constants.h"
#include "Snowflake.h"
#include "Snowpile.h"

#define SNOW_NUM_FLAKES 20
#define SNOW_NEXT_FLAKE_TRIGGER 250
#define SNOW_INIT_GLINT_VALUE 105
#define SNOW_GLINT_STEP 11

#define PILE_R 150
#define PILE_G 150
#define PILE_B 150
/*
#define BG_R 27
#define BG_G 39
#define BG_B 86
*/
#define BG_R 24
#define BG_G 43
#define BG_B 115


class PatternSnow : public Pattern {

  uint8_t intensity;
  
  Snowflake flakes[SNOW_NUM_FLAKES];  
  Snowpile pile;

  // for fading out  
  uint8_t flakeBrightness;

  uint8_t nextFlakeCounter;
  uint8_t nextFlakeStep;
    
  uint8_t glintPixel;
  uint8_t glintValue;
  
  inline uint8_t newStep(uint8_t intensity) __attribute__((always_inline))  {
    return random8((intensity >> 1) + 1, ((intensity + 4) * 3) >> 2);
  }

public:
  PatternSnow();
  void run(byte intensity);
};

#endif
