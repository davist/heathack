
#ifndef __INC_MOOD_SELECTED_H
#define __INC_MOOD_SELECTED_H

#include "Mood.h"
#include "Palettes.h"
#include "config.h"


static const uint32_t INDEX_DURATION = 30000;
static const uint32_t STEADY_PALETTE_DURATION  = 300000;

#define CHANGE_FRAME_DELAY 5


class MoodReport : public Mood {
  
  bool running;
  uint8_t step;
  uint8_t frameDelayCount;
  uint8_t curLedNum;
  uint8_t curFadeStep;

public:
  MoodReport() {
    uint8_t paletteSize = palettes[SELECT_PALETTE].size;
    this->colour = palettes[SELECT_PALETTE].colours[index % paletteSize];
    curLedNum = 0;
    running = true;
    step = 0;
    running = CHANGE_FRAME_DELAY;
    frameDelayCount = 0;
  }
  
  // slower rate of change than default
  uint8_t fadeStep() { return 60; }
  
  bool run(void) {

    if (!running) return false;

    switch (step) {
      case 0:
        // filling up
        if (!doChangeStep()) {
          // finished filling up
          step = 1;
        }
        break;

      case 1:
        // flash white
        if (!doFlashStep()) {
          step = 2;
        }
        break;
      case 2:
        // back to colour
        if (!doReturnStep()) {
          running = false;
        }
        break;
    }
    
    return true;
  }
  
private:

bool doChangeStep(void) {
  // wait for frameDelay frames before executing next step
  if (++frameDelayCount < CHANGE_FRAME_DELAY) return true;

  frameDelayCount = 0;

  leds[curLedNum++] = colour;

  if (curLedNum == NUM_LEDS) {
    // finished
    return false;
  }
  else {
    // keep changing
    return true;
  }
}

bool doFlashStep(void) {
  // wait for frameDelay frames before executing next step
  if (++frameDelayCount < CHANGE_FRAME_DELAY) return true;

  frameDelayCount = 0;

  fill_solid(leds, NUM_LEDS, CRGB::White);
  curFadeStep = 60;

  return false;
}
  
bool doReturnStep(void) {
  // wait for frameDelay frames before executing next step
  if (++frameDelayCount < CHANGE_FRAME_DELAY) return true;

  frameDelayCount = 0;

  fill_solid(leds, NUM_LEDS, colour);
  curFadeStep = 20;

  return false;
}

};

#endif

