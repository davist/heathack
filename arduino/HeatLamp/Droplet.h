#ifndef __INC_DROPLET_H
#define __INC_DROPLET_H

#include "FastLED.h"
#include "Accumulation.h"
#include "PrecipConfig.h"


class Droplet {

  uint16_t height; // 8 times num pixels to allow of 1/8 sub-pixel positioning
  int8_t speed;
  uint8_t angle;
  uint8_t brightness;
  bool isFastRotation;
  bool isSplashing;
  bool isActive;
    
  //uint8_t pow8(uint8_t n, uint8_t e);
  
public:

  static const PrecipConfig* config;

  Droplet();
  void init();
  void update(Accumulation* pile);
  void render();
  inline bool active() __attribute__((always_inline))  {
    return isActive;
  }
};

#endif
