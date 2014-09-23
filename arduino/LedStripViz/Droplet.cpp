#include "Droplet.h"
#include <math.h>
#include "constants.h"

Droplet::Droplet() {
  isActive = false;
}

const PrecipConfig* Droplet::config;

// initialise the droplet at the top and start it falling
// (all droplets are created up front instead of as needed)
void Droplet::init() {
  height = NUM_LEDS * HEIGHT_PER_LED - 1;
  speed = config->dropletInitSpeed;
  angle = random8();
  brightness = random8(config->dropletMinBrightness, 255);
  isSplashing = false;
  isFastRotation = random8() < 128;
  isActive = true;
}

void Droplet::update(Accumulation* pile) {
  if (!isActive) return;
  
  // update pos and rotation
  height = height > speed ? height - speed : 0;
  speed = qadd7(speed, config->dropletAcceleration);
  angle += isFastRotation ? config->dropletRotationFast : config->dropletRotationSlow; //(Math.random() / 10 + 0.05);
  
  if (isSplashing) {
    if (brightness == 0 ||
        height <= pile->height) {
          
      isActive = false;
    }
    
    brightness = qsub8(brightness, 10);
  }    
  
  // has it reached top of pile?
  if (height <= pile->height && !isSplashing) {
      // add droplet to pile
      pile->addDroplet();
      
      if (config->pileSplashSpeed > 0) {
        isSplashing = true;
        height = pile->height;
        speed = -config->pileSplashSpeed;
      }
      else {
        isActive = false;
      }
  }
}

// raise n to the power e, treating n as a fraction from 0/256 to 255/256
uint8_t Droplet::pow8(uint8_t n, uint8_t e) {
  uint8_t result = n;
  while (e > 1) {
    result = scale8(result, n);
    e--;
  }
  return result;
}

void Droplet::render() {
  if (!isActive) return;

  uint8_t value = config->dropletColour.v + 
                  scale8(pow8(cos8(angle), config->dropletShininess), config->dropletValueRange);

  // scale value down by overall brightness
  value = scale8(value, brightness);

  // use anti-aliasing to simulate sub-pixel positioning
  
  // round down to nearest multiple of HEIGHT_PER_LED
  uint8_t start = height >> HEIGHT_SHIFT;

  uint8_t aaAmount = (height & HEIGHT_MASK) << AA_SCALE_SHIFT;
  
  // blend into existing pixel based on aaAmount
  nblend(leds[start],
         CHSV(config->dropletColour.h, config->dropletColour.s, value),
         255 - aaAmount);

  if (start < NUM_LEDS - 1) {
    nblend(leds[start+1],
           CHSV(config->dropletColour.h, config->dropletColour.s, value),
           aaAmount);
  }
}


