#ifndef __INC_SNOWPILE_H
#define __INC_SNOWPILE_H

#include <stdint.h>

#define PILE_MELT_RATE 1
#define PILE_GROW_RATE 3
#define PILE_FLAKE_SIZE 11

struct Snowpile {

  // ten times num pixels to allow of 1/8 sub-pixel positioning
  
  // current height
  uint16_t height;

  // overall target height that pile is moving towards (based on intensity)
  uint16_t target;

  // each time a flake is added it increments the interimTarget.
  // The height then moves gradually towards iT
  uint16_t interimTarget;

  
  Snowpile() {
    height = 0;
    target = 0;
    interimTarget = 0;
  }
  
  void addFlake() {
      if (interimTarget < target) {
          interimTarget += PILE_FLAKE_SIZE;
          if (interimTarget > target) interimTarget = target;
      }
  }
          
  void adjust() {
      if (height > target) {
          // melt
          height -= PILE_MELT_RATE;
          interimTarget = height;
          if (height < target) height = target;
      }
      else if (height < interimTarget) {
          // grow
          height += PILE_GROW_RATE;
          if (height > interimTarget) height = interimTarget;                
      }
  }
};

#endif
