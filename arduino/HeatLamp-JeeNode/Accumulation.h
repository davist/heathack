#ifndef __INC_ACCUMULATION_H
#define __INC_ACCUMULATION_H

#include <stdint.h>
#include "constants.h"
#include "PrecipConfig.h"

#define MIN_LEDS_FOR_GLINT 8


struct Accumulation {

  // ten times num pixels to allow of 1/8 sub-pixel positioning
  
  // current height
  uint16_t height;

  // overall target height that pile is moving towards (based on intensity)
  uint16_t target;

  // each time a particle is added it increments the interimTarget.
  // The height then moves gradually towards iT
  uint16_t interimTarget;

  uint8_t glintPixel;
  uint8_t glintValue;
  
  uint8_t wavePhase;
  uint8_t waveHeight;
  uint8_t waveFreq;
  
  // growViaDroplets;
  // true - pile only grows toward target when droplets are added
  // false - grows as soon as target is increased - droplets are irrelevant


  const PrecipConfig* config;
  
  Accumulation() {
    height = 0;
    target = 0;
    interimTarget = 0;
    glintPixel = 0;
    glintValue = 0;
    wavePhase = 0;
    waveHeight = config->pileWaveHeight;
    waveFreq = config->pileWaveFreq;
  }
  
  void setTarget(uint16_t target) {
    this->target = target;
    if (config->pileGrowViaDroplets) interimTarget = target;
  }
  
  void addDroplet() {
      if (interimTarget < target) {
          interimTarget += config->pileDropletIncrement;
          if (interimTarget > target) interimTarget = target;
      }
      
      waveHeight = config->pileWaveSplashHeight + config->pileWaveStep;
      waveFreq = config->pileWaveSplashFreq + config->pileWaveStep;
  }
          
  void adjust() {
      if (height > target) {
          // melt
          height -= config->pileMeltRate;
          interimTarget = height;
          if (height < target) height = target;
      }
      else if (height < interimTarget) {
          // grow
          height += config->pileGrowRate;
          if (height > interimTarget) height = interimTarget;                
      }
      
      if (waveHeight > config->pileWaveHeight) {
        waveHeight -= config->pileWaveStep;
        if (waveHeight < config->pileWaveHeight) waveHeight = config->pileWaveHeight;
      }

      if (waveFreq > config->pileWaveFreq) {
        waveFreq -= config->pileWaveStep;
        if (waveFreq < config->pileWaveFreq) waveFreq = config->pileWaveFreq;
      }
  }
  
  uint8_t render(uint8_t bgBrightness = 0) {
    uint16_t adjHeight;
    
    // if waves enabled, adjust height;
    if (config->pileWaveHeight > 0) {

      int8_t wave = scale8(sin8(wavePhase), waveHeight << 1) - waveHeight;

      if (wave < 0 && abs8(wave) > height ) {
        adjHeight = 0;
      }
      else {
        adjHeight = height + wave;
      }
      
      wavePhase += waveFreq;
    }
    else {
      adjHeight = height;
    }

    uint8_t pileEnd = adjHeight >> HEIGHT_SHIFT;
    if (pileEnd > NUM_LEDS) pileEnd = NUM_LEDS;
    
    for (uint8_t i=0; i<pileEnd; i++) {
        leds[i].setRGB(config->pileColour.r, config->pileColour.g, config->pileColour.b);
    }
    
    // new glint?
    if (glintValue == 0 && pileEnd >= MIN_LEDS_FOR_GLINT) {
      if (random8(0, 100) > (100 - config->glintChance)) {
        glintValue = 255;
        glintPixel = (glintPixel + random8(1, pileEnd - 1)) % pileEnd;
      }
    }
    
    if (glintValue > 0) {

      uint8_t glintBrightness = pow8(sin8(glintValue), 10);
      
      leds[glintPixel] += CRGB(scale8_video(config->glintColour.r, glintBrightness),
                               scale8_video(config->glintColour.g, glintBrightness), 
                               scale8_video(config->glintColour.b, glintBrightness));

      glintValue = qsub8(glintValue, config->glintStep);
    }
    
    // anti-alias the top pixel in the pile
    
    // set bg colour
    if (pileEnd < NUM_LEDS) {
      leds[pileEnd].setRGB(scale8_video(config->bgColour.r, bgBrightness),
                           scale8_video(config->bgColour.g, bgBrightness), 
                           scale8_video(config->bgColour.b, bgBrightness));
  
      // blend in pile colour
      nblend(leds[pileEnd],
             CRGB(config->pileColour.r, config->pileColour.g, config->pileColour.b),
             (adjHeight & HEIGHT_MASK) << AA_SCALE_SHIFT);
    }
    
    return pileEnd;
  }
};

#endif
