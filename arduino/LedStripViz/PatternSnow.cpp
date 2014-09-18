#include "PatternSnow.h"

PatternSnow::PatternSnow() {

  intensity = 99;
  
  nextFlakeCounter = SNOW_NEXT_FLAKE_TRIGGER;
  nextFlakeStep = 0;
  glintPixel = 0;
  glintValue = 0;
  flakeBrightness = 255;
}

void PatternSnow::run(uint8_t intensity) { 

  uint8_t i;
  
  if (intensity == 0) qsub8(flakeBrightness, 6);
  
  // set pile target height based on intensity (5 - 45)
  if (intensity != this->intensity) {
      this->intensity = intensity;
      
      if (intensity == 0) {
        //fade to black
        pile.target = 0;
        nextFlakeStep = 0; // prevent flakes starting
      }
      else {
        intensity--;
        pile.target = (5 + intensity * 4) * HEIGHT_PER_LED;
        
        // recalc next flake step size
        nextFlakeStep = newStep(intensity);
      }
  }

  // new flake?
  if (nextFlakeCounter >= SNOW_NEXT_FLAKE_TRIGGER) {
  
      // find inactive flake and activate it
      for (i=0; i < SNOW_NUM_FLAKES; i++) {
          if (!flakes[i].active()) {
              flakes[i].init();
              break;
          }
      }
  
      nextFlakeCounter = 0;
      nextFlakeStep = newStep(intensity);
  }
  else {
      nextFlakeCounter += nextFlakeStep;
  }

  // update flakes
  for (i=0; i < SNOW_NUM_FLAKES; i++) {
    flakes[i].update(&pile);
  }

  pile.adjust();
  
  // render pile
  uint8_t pileEnd = pile.height >> HEIGHT_SHIFT;
  for (i=0; i<pileEnd; i++) {
      leds[i].setRGB(PILE_R, PILE_G, PILE_B);
  }
  
  // new glint?
  if (glintValue == 0 && pileEnd > 0) {
    if (random8(0, 50) == 25) {
      glintValue = SNOW_INIT_GLINT_VALUE;
      glintPixel = random8(0, pileEnd);
    }
  }
  
  if (glintValue > 0) {
      leds[glintPixel].addToRGB(glintValue);
      glintValue = qsub8(glintValue, SNOW_GLINT_STEP);
  }
  
  // render background
  uint8_t brightness = (pile.height * 7) / 10;
  for (i=pileEnd; i < NUM_LEDS; i++) {
      leds[i].setRGB(scale8_video(BG_R, brightness),
                     scale8_video(BG_G, brightness), 
                     scale8_video(BG_B, brightness));
  }
  
  // anti-alias the top pixel in the pile
  nblend(leds[pileEnd],
         CRGB(PILE_R, PILE_G, PILE_B),
         (pile.height & HEIGHT_MASK) << AA_SCALE_SHIFT);
  
  // render flakes
  for (i=0; i < SNOW_NUM_FLAKES; i++) {
      flakes[i].render(flakeBrightness);
  }
}

