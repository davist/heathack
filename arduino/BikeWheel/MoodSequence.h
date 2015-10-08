/*
 * MoodSequence.h
 *
 *  Created on: 7 Oct 2015
 *      Author: tim
 */

#ifndef MOODSEQUENCE_H_
#define MOODSEQUENCE_H_

#include "Mood.h"
#include "Palettes.h"
#include "config.h"

const uint8_t ALL_LEDS = 255;

/**
 * A mood that is controlled by a simple sequence
 * Each step specifies: ldeIndex, colour, duration, fadeStep
 *
 * Abstract class. Must be extended with a class that
 * defines the sequence to create an actual mood.
 * This class just controls the running of the mood.
 */
class MoodSequence : public Mood {

protected:

  struct SeqStep {
    uint8_t ledIndex;  // 255 for all
    uint8_t colourIndex;  // pointer into palette
    uint8_t duration; // duration in frames
    uint8_t fadeStep;   // rate that LEDs fade to next selected colour
  };

private:

  bool running;
  uint8_t stepNum;
  uint8_t frameDelayCount;
  uint8_t curFadeStep;

  SeqStep *curSequence;
  uint8_t seqSize;
  CRGB *curPalette;

public:
  MoodSequence(SeqStep *sequence, CRGB *palette, uint8_t seqSize) {
    running = true;
    stepNum = 0;
    curFadeStep = 0;
    frameDelayCount = 0;
    this->seqSize = seqSize;
    curSequence = sequence;
    curPalette = palette;
  }

  // slower rate of change than default
  uint8_t fadeStep() { return curFadeStep; }

  bool run(void) {

    if (!running) return false;

    // wait for frameDelay frames before executing next step
    if (frameDelayCount-- > 0) return true;

    if (stepNum >= seqSize) {
      // reached end of sequence
      running = false;
    }
    else {
      // read next step settings
      SeqStep *curStep = &curSequence[stepNum];

      frameDelayCount = curStep->duration;
      curFadeStep = curStep->fadeStep;

      // all leds or just one
      if (curStep->ledIndex == ALL_LEDS) {
        fill_solid(leds, NUM_LEDS, curPalette[curStep->colourIndex]);
      }
      else {
        leds[curStep->ledIndex] = curPalette[curStep->colourIndex];
      }

      // move onto next step
      stepNum++;
    }

    return true;
  }

};

#endif /* MOODSEQUENCE_H_ */
