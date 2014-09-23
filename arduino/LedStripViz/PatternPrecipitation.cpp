#include "PatternPrecipitation.h"

PatternPrecipitation::PatternPrecipitation(const PrecipConfig* config) {

  this->config = config;
/*
  Serial.print("pileColour.r ");  
  Serial.println(config->pileColour.r);

  Serial.print("pileColour.g ");  
  Serial.println(config->pileColour.g);

  Serial.print("pileColour.b ");  
  Serial.println(config->pileColour.b);
  
  Serial.print("bgColour.r ");  
  Serial.println(config->bgColour.r);

  Serial.print("bgColour.g ");  
  Serial.println(config->bgColour.g);

  Serial.print("bgColour.b ");  
  Serial.println(config->bgColour.b);
  
  Serial.print("dropletColour.h ");  
  Serial.println(config->dropletColour.h);

  Serial.print("dropletColour.s ");  
  Serial.println(config->dropletColour.s);

  Serial.print("dropletColour.v ");  
  Serial.println(config->dropletColour.v);
  
  Serial.print("nextDropletTrigger ");  
  Serial.println(config->nextDropletTrigger);
  
  Serial.print("glintValue ");  
  Serial.println(config->glintValue);
  
  Serial.print("glintStep ");  
  Serial.println(config->glintStep);
  
  Serial.print("pileGrowViaDroplets ");  
  Serial.println(config->pileGrowViaDroplets);
  
  Serial.print("pileMeltRate ");  
  Serial.println(config->pileMeltRate);
  
  Serial.print("pileGrowRate ");  
  Serial.println(config->pileGrowRate);
  
  Serial.print("pileDropletIncrement ");  
  Serial.println(config->pileDropletIncrement);
  
  Serial.print("pileSplashHeight ");  
  Serial.println(config->pileSplashHeight);
  
  Serial.print("pileWaveHeight ");  
  Serial.println(config->pileWaveHeight);
  
  Serial.print("pileWaveFreq ");  
  Serial.println(config->pileWaveFreq);
  
  Serial.print("pileWaveSplashHeight ");  
  Serial.println(config->pileWaveSplashHeight);
  
  Serial.print("pileWaveSplashFreq ");  
  Serial.println(config->pileWaveSplashFreq);
  
  Serial.print("pileWaveStep ");  
  Serial.println(config->pileWaveStep);
  
  Serial.print("dropletInitSpeed ");  
  Serial.println(config->dropletInitSpeed);
  
  Serial.print("dropletAcceleration ");  
  Serial.println(config->dropletAcceleration);
  
  Serial.print("dropletShininess ");  
  Serial.println(config->dropletShininess);
  
  Serial.print("dropletRotationFast ");  
  Serial.println(config->dropletRotationFast);
  
  Serial.print("dropletRotationSlow ");  
  Serial.println(config->dropletRotationSlow);
  
  Serial.print("dropletValueRange ");  
  Serial.println(config->dropletValueRange);
*/
  intensity = 99;
  
  nextDropletCounter = config->nextDropletTrigger;
  nextDropletStep = 0;
 // dropletBrightness = 255;
  
  pile.config = config;
  Droplet::config = config;
}

void PatternPrecipitation::run(uint8_t intensity) { 

  uint8_t i;
  
  //if (intensity == 0) qsub8(dropletBrightness, 6);
  
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


const PrecipConfig PrecipSnow = {
  {150, 150, 150},   // pileColour
  {24, 43, 115},     // bgColour
  {128, 0, 150},     // dropletColour
  {105, 105, 105},   // glintColour
  
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
//  {24, 43, 115},   // pileColour
  {43, 75, 255},   // pileColour
  {0, 0, 0},       // bgColour
  {150, 255, 255}, // dropletColour
  {0, 0, 0},       // glintColour
  
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

