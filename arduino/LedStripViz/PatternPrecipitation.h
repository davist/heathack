#ifndef __INC_PATTERN_PRECIPITATION_H
#define __INC_PATTERN_PRECIPITATION_H

#include "Pattern.h"
#include "constants.h"
#include "Droplet.h"
#include "Accumulation.h"
#include "PrecipConfig.h"

#define PRECIP_NUM_DROPLETS 20

class PatternPrecipitation : public Pattern {

  const PrecipConfig* config;
  
  uint8_t intensity;
  
  Droplet droplets[PRECIP_NUM_DROPLETS];  
  Accumulation pile;

  // for fading out  
 // uint8_t dropletBrightness;

  uint8_t nextDropletCounter;
  uint8_t nextDropletStep;
    
  inline uint8_t newStep(uint8_t intensity) __attribute__((always_inline))  {
    return random8((intensity >> 1) + 1, ((intensity + 4) * 4) >> 2);
  }

public:
  PatternPrecipitation(const PrecipConfig* config);
  void run(byte intensity);
};

#endif
