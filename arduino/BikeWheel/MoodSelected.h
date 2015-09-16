
#ifndef __INC_MOOD_SELECTED_H
#define __INC_MOOD_SELECTED_H

#include "Mood.h"
#include "Palettes.h"
#include "config.h"


static const uint32_t INDEX_DURATION = 30000;
static const uint32_t STEADY_PALETTE_DURATION  = 300000;

#define CHANGE_FRAME_DELAY 5


class MoodSelected : public Mood {
  
  bool fillingUp;
  uint8_t frameDelayCount;
  CRGB colour;
  uint8_t curLedNum;

public:
  MoodSelected(CRGB colour) {
    this->colour = colour;
    curLedNum = 0;
    fillingUp = true;
    frameDelayCount = CHANGE_FRAME_DELAY;
  }
  
  // slower rate of change than default
  uint8_t fadeStep() { return 10; }
  
  bool run(void) {
    
    if (fillingUp) {
      fillingUp = doChangeStep();
      return true;
    }
    else {
      return false;
    }
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

  
};

#endif

