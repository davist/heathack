/*
    FairgroundStar. Firmware for an Arduino-based star-shaped fairground/circus-style LED light.
    Copyright (C) 2015  Tim Davis

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __INC_MOOD_TWINKLE_H
#define __INC_MOOD_TWINKLE_H

#include "Mood.h"
#include "Palettes.h"
#include "config.h"


static const uint32_t TWINKLE_PALETTE_DURATION = 30000;

struct Bulb {
  bool fadingUp;
  uint8_t value;
  uint8_t step;
  uint8_t paletteIndex;
  CRGB colour;
  uint8_t ledNum;
  
  void init(uint8_t step, uint8_t curPalette, uint8_t ledNum) {
    this->fadingUp = true;
    this->value = random8();
    this->step = step;
    this->paletteIndex = ledNum % palettes[curPalette].size;
    this->colour = palettes[curPalette].colours[paletteIndex];
    this->ledNum = ledNum;
  }
  
  void update(uint8_t curPalette) {

    // update value
    if (fadingUp) {
      value = qadd8(value, step);
      if (value == 255) {
        fadingUp = false;
      }
    }
    else {
      value = qsub8(value, step);
      if (value == 0) {
        fadingUp = true;
        paletteIndex = (paletteIndex + 1) % palettes[curPalette].size;
        // re-get colour from palette even if not cycling in case palette has changed
        colour = palettes[curPalette].colours[paletteIndex];        
      }
    }
    
    // render
    
    // get current colour
    CRGB col = colour;
    
    // scale by value
    col %= pow8(value, 2);
    
    // set relevant led
    leds[ledNum] = col;
  }
  
  // raise n to the power e, treating n as a fraction from 0/256 to 255/256
  inline uint8_t pow8(uint8_t n, uint8_t e) {
    uint8_t result = n;
    while (e > 1) {
      result = scale8(result, n);
      e--;
    }
    return result;
  }

};


class MoodTwinkle : public Mood {
  
  uint8_t curPalette;
  uint32_t lastPaletteChangeTime;
  
  Bulb bulbs[NUM_LEDS];

public:
  MoodTwinkle() {
    lastPaletteChangeTime = millis();
    changePalette();
    init();    
  }
  
  bool run(void) {
    // time to change palette?
    if (millis() - lastPaletteChangeTime > TWINKLE_PALETTE_DURATION) {
      lastPaletteChangeTime = millis();
      changePalette();
    }

    // update bulbs
    for (int8_t i=0; i<NUM_LEDS; i++) {
      bulbs[i].update(curPalette);
    }
    
    return true;
  }
 
 private:
 
  void changePalette(void) {
    curPalette = random8(NUM_PALETTES);
  }

  void init(void) {
    // slow twinkle
    for (int8_t i=0; i<NUM_LEDS; i++) {
      bulbs[i].init(6 + i, curPalette, i);
    }
  }
};

#endif

