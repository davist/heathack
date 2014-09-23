#ifndef __INC_PRECIP_CONFIG_H
#define __INC_PRECIP_CONFIG_H

// height per led must be power of 2 to allow for bit-shifting arithmetic
#define HEIGHT_PER_LED 8
#define HEIGHT_MASK 7
#define HEIGHT_SHIFT 3
#define AA_SCALE_SHIFT 5

struct PrecipConfig {

  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
  } pileColour;
  
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
  } bgColour;
  
  struct {
    uint8_t h;
    uint8_t s;
    uint8_t v;
    
  } dropletColour;
  
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
  } glintColour;

  uint8_t nextDropletTrigger;
  
  uint8_t glintStep;
  uint8_t glintChance;
  
  bool pileGrowViaDroplets;
  uint8_t pileMeltRate;
  uint8_t pileGrowRate;
  uint8_t pileDropletIncrement;
  uint8_t pileSplashSpeed;
  uint8_t pileWaveHeight;
  uint8_t pileWaveFreq;
  uint8_t pileWaveSplashHeight;
  uint8_t pileWaveSplashFreq;
  uint8_t pileWaveStep;
  
  
  uint8_t dropletInitSpeed;
  uint8_t dropletAcceleration;
  uint8_t dropletShininess;
  uint8_t dropletRotationFast;
  uint8_t dropletRotationSlow;
  uint8_t dropletValueRange;
  uint8_t dropletMinBrightness;

};

extern const PrecipConfig PrecipRain;
extern const PrecipConfig PrecipSnow;

#endif

