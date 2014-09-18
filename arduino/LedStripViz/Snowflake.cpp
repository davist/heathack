#include "Snowflake.h"
#include <math.h>
#include "constants.h"

Snowflake::Snowflake() {
  isActive = false;
}

// initialise the snowflake at the top and start it falling
// (all flakes are created up front instead of as needed)
void Snowflake::init() {
  height = NUM_LEDS * HEIGHT_PER_LED - 1;
  angle = random8();
  isFastRotation = random8() < 128;
  isActive = true;
}

void Snowflake::update(Snowpile* pile) {
  if (!isActive) return;
  
  // update pos and rotation
  height -= FLAKE_SPEED;
  angle += isFastRotation ? 5 : 4; //(Math.random() / 10 + 0.05);
  
  // has it reached top of pile?
  if (height <= pile->height) {
      // add flake to pile
      pile->addFlake();
      isActive = false;
  }
}

// raise n to the power e, treating n as a fraction from 0/256 to 255/256
uint8_t Snowflake::pow8(uint8_t n, uint8_t e) {
  uint8_t result = n;
  while (e > 1) {
    result = scale8(result, n);
    e--;
  }
  return result;
}

void Snowflake::render(uint8_t brightness) {
  if (!isActive) return;

  uint8_t value = FLAKE_VALUE_MIN + 
                  scale8(pow8(cos8(angle), FLAKE_SHININESS), FLAKE_VALUE_RANGE);

  // scale value down by overall brightness (for fading out)
  value = scale8(value, brightness);

  // use anti-aliasing to simulate sub-pixel positioning
  
  // round down to nearest multiple of HEIGHT_PER_LED
  uint8_t start = height >> HEIGHT_SHIFT;

  uint8_t aaAmount = (height & HEIGHT_MASK) << AA_SCALE_SHIFT;
  
  // blend into existing pixel based on aaAmount
  nblend(leds[start],
         CHSV(FLAKE_HUE, FLAKE_SATURATION, value),
         255 - aaAmount);

  if (start < NUM_LEDS - 1) {
    nblend(leds[start+1],
           CHSV(FLAKE_HUE, FLAKE_SATURATION, value),
           aaAmount);
  }
}


