#include "PatternPrecipitation.h"

PatternPrecipitation::PatternPrecipitation(const PrecipConfig* config) {

  this->config = config;
  intensity = 99;
  
  nextDropletCounter = config->nextDropletTrigger;
  nextDropletStep = 0;
  
  pile.config = config;
  Droplet::config = config;
}

void PatternPrecipitation::run(uint8_t intensity) { 

  uint8_t i;
  
  // set pile target height based on intensity (5 - 45)
  if (intensity != this->intensity) {
      this->intensity = intensity;
      
      if (intensity == 0) {
        //fade to black
        pile.setTarget(0);
        nextDropletStep = 0; // prevent droplets starting
      }
      else {
        intensity--;
        pile.setTarget( (5 + intensity * 4) * HEIGHT_PER_LED );
        
        // recalc next droplet step size
        nextDropletStep = newStep(intensity);
      }
  }

  // new droplet?
  if (nextDropletCounter >= config->nextDropletTrigger) {
  
      // find inactive droplet and activate it
      for (i=0; i < PRECIP_NUM_DROPLETS; i++) {
          if (!droplets[i].active()) {
              droplets[i].init();
              break;
          }
      }
  
      nextDropletCounter = 0;
      nextDropletStep = newStep(intensity);
  }
  else {
      nextDropletCounter = qadd8(nextDropletCounter, nextDropletStep);
  }

  // update droplets
  for (i=0; i < PRECIP_NUM_DROPLETS; i++) {
    droplets[i].update(&pile);
  }

  pile.adjust();
  
  uint8_t bgBrightness = (pile.height * 7) / 10;

  uint8_t pileEnd = pile.render(bgBrightness);
  
  // render background
  for (i=pileEnd + 1; i < NUM_LEDS; i++) {
      leds[i].setRGB(scale8_video(config->bgColour.r, bgBrightness),
                     scale8_video(config->bgColour.g, bgBrightness), 
                     scale8_video(config->bgColour.b, bgBrightness));
  }
  
  // render droplets
  for (i=0; i < PRECIP_NUM_DROPLETS; i++) {
      droplets[i].render();
  }
}


// raise n to the power e, treating n as a fraction from 0/256 to 255/256
uint8_t pow8(uint8_t n, uint8_t e) {
  uint8_t result = n;
  while (e > 1) {
    result = scale8(result, n);
    e--;
  }
  return result;
}

const PrecipConfig PrecipSnow = {
  {150, 150, 150},   // pileColour
  {4, 8, 20},     // bgColour
  {128, 0, 150},     // dropletColour
  {255, 255, 255},   // glintColour
  
  250, // nextDropletTrigger
  
  11, // glintStep;
  3,  // glintChance
  
  true, // pileGrowViaDroplets
  1, // pileMeltRate;
  3, // pileGrowRate;
  11, // pileDropletIncrement;
  0, // pileSplashSpeed;
  0, // pileWaveHeight;
  0, // pileWaveFreq;
  0, // pileWaveSplashHeight;
  0, // pileWaveSplashFreq;
  0, // pileWaveStep;

  
  1, // dropletInitSpeed;
  0, // dropletAcceleration;
  20, // dropletShininess;
  4, // dropletRotationFast;
  5, // dropletRotationSlow;
  105, // dropletValueRange (255 - dropletColour.v)
  205 // dropletMinBrightness
};

const PrecipConfig PrecipRain = {
  {43, 75, 255},   // pileColour
  {0, 0, 0},       // bgColour
  {150, 255, 255}, // dropletColour
  {255, 255, 255},       // glintColour
  
  150, // nextDropletTrigger
  
  5, // glintStep;
  2,  // glintChance
  
  true, // pileGrowViaDroplets
  2, // pileMeltRate;
  2, // pileGrowRate;
  10, // pileDropletIncrement;
  15, // pileSplashSpeed;
  5, // pileWaveHeight;
  5, // pileWaveFreq;
  15, // pileWaveSplashHeight;
  20, // pileWaveSplashFreq;
  1, // pileWaveStep;

  
  0, // dropletInitSpeed;
  3, // dropletAcceleration;
  1, // dropletShininess;
  3, // dropletRotationFast;
  4, // dropletRotationSlow;
  0, // dropletValueRange (255 - dropletColour.v)
  130 // dropletMinBrightness

};

