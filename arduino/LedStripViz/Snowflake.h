#ifndef __INC_SNOWFLAKE_H
#define __INC_SNOWFLAKE_H

#include "Snowpile.h"
#include "FastLED.h"

// height per led must be power of 2 to allow for bit-shifting arithmetic
#define HEIGHT_PER_LED 8
#define HEIGHT_MASK 7
#define HEIGHT_SHIFT 3
#define AA_SCALE_SHIFT 5

#define FLAKE_SPEED 1
#define FLAKE_SHININESS 20
#define FLAKE_HUE 128
#define FLAKE_SATURATION 0
#define FLAKE_VALUE_MIN 150
#define FLAKE_VALUE_RANGE (255 - 150)

class Snowflake {

  uint16_t height; // 8 times num pixels to allow of 1/8 sub-pixel positioning
  uint8_t angle;
  bool isFastRotation;
  bool isActive;
  
  uint8_t pow8(uint8_t n, uint8_t e);
  
public:
  Snowflake();
  void init();
  void update(Snowpile* pile);
  void render(uint8_t brightness);
  inline bool active() __attribute__((always_inline))  {
    return isActive;
  }
};

#endif
