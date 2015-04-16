#ifndef __INC_PATTERN_FIRE_2012_H
#define __INC_PATTERN_FIRE_2012_H

#include "Pattern.h"
#include "constants.h"

class PatternFire2012 : public Pattern {

  // Array of temperature readings at each simulation cell
  uint8_t heat[NUM_LEDS]; 

  byte intensity;
  byte cooling;
  byte hotCooling;
  byte sparking;
  byte sparkZone;
  byte sparkTempMin;
  byte sparkTempMax;

  byte maxCooling;
  byte maxHotCooling;
  
public:
  PatternFire2012();
  ~PatternFire2012();
  void run(byte intensity);
 
 private: 
 
  void setIntensity(byte intensity);
};

#endif
