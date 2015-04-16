#ifndef __INC_PATTERN_H
#define __INC_PATTERN_H

#include "FastLED.h"

class Pattern {
public:
  virtual void run(byte intensity) = 0;
};

#endif

