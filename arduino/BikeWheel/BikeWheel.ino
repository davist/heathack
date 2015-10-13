#include <FastLED.h>
#include "config.h"
#include "NonProgrammableLEDController.h"
#include "Mood.h"
#include "MoodTwinkle.h"
#include "MoodSelected.h"
#include "MoodReport.h"

//-------------------------------------------------------------------------------------------
// BEGIN CONFIGURATION
     
// what are our base colours for our colour scheme?
const CRGB basecolour[NUM_LEDS] = {CRGB::White, CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Cyan};

// FastLED controller for simple, non-programmable LEDs
NonProgrammableLEDController controller(
    RED_PIN, GREEN_PIN, BLUE_PIN,
    COMMON_PINS,
    NUM_LEDS
    , COMMON_CATHODE
#if DEBUG  // slow down multiplexing
    , 0
    , 1000000
#endif
);

// Define the array of leds that the moods write to
CRGB leds[NUM_LEDS];

// and the array of actual colours that FastLED uses.
// leds is used to update realLeds every frame, fading from
// the current colour (realLeds) to the required colour (leds).
// This is to simulate the response time of incandesent bulbs.
CRGB realLeds[NUM_LEDS];

enum MoodID { None, Twinkle, Selected, Report };

Mood* curMood;
MoodID curMoodID = None;

// record number of times each input is selected
uint8_t scores[NUM_LEDS];


//-------------------------------------------------------------------------
// copy leds to realLeds with hysteresis
inline void copyLedsToRealLeds(uint8_t step) {

  for (uint8_t i=0; i<NUM_LEDS; i++) {
    CRGB *current = &realLeds[i];
    CRGB *target = &leds[i];
    CRGB oldCurrent = *current;

    // use linear interpolation to move from current colour towards target colour
    current->r = lerp8by8(current->r, target->r, step * 2);
    current->g = lerp8by8(current->g, target->g, step * 2);
    current->b = lerp8by8(current->b, target->b, step * 2);

    // if no change, force to target value
    if (*current == oldCurrent) {
      *current = *target;
    }    
  }
}


//-------------------------------------------------------------------------
void setMood(MoodID id, uint8_t index = 0) {

  // only change mood if requested one is different to current one
  if (curMoodID == id) return;

  delete curMood;

#if DEBUG
  Serial.print("set mood to ");
  Serial.print(id);
  Serial.print(", ");
  Serial.println(index);
#endif

  // clear down all leds
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  switch (id) {
  case Twinkle:
    curMood = new MoodTwinkle();
    break;
  case Selected: {
    curMood = new MoodSelected(index);
    break;
  }
  case Report:
    static uint8_t rankings[NUM_LEDS];
    static uint8_t scoreSort[NUM_LEDS];

    // calculate current rankings from scores

    // initialise rankings / scoreSort
    for (uint8_t r=0; r<NUM_LEDS; r++) {
      scoreSort[r] = scores[r];

      // only include scores > 0
      if (scores[r] > 0) {
        rankings[r] = r;
      }
      else {
        rankings[r] = 255;
      }
    }

    // sort scores
    for(uint8_t i=0; i<(NUM_LEDS-1); i++) {
      for(uint8_t o=0; o<(NUM_LEDS-(i+1)); o++) {
        if(scoreSort[o] < scoreSort[o+1]) {
          swap(&scoreSort[o], &scoreSort[o+1]);
          swap(&rankings[o], &rankings[o+1]);
        }
      }
    }

    curMood = new MoodReport(rankings);
    break;
  case None:
    break;
  }

  curMoodID = id;
}

void swap(uint8_t *a, uint8_t *b) {
  (*a) ^= (*b);
  (*b) ^= (*a);
  (*a) ^= (*b);
}

//-------------------------------------------------------------------------
void setup() { 
#if DEBUG
     Serial.begin(57600);
     Serial.println("BikeWheel");
#endif

    // enable pull-up resistors on input pins so that the "switch" only has to
    // connect the input to ground
    for (byte i=0; i<NUM_LEDS; i++) {
      pinMode(INPUT_PINS[i], INPUT_PULLUP);
    }

     FastLED.addLeds(&controller, realLeds, NUM_LEDS);
     FastLED.setDither(DISABLE_DITHER);
     FastLED.setCorrection(Typical8mmPixel);

     // start with twinkling
     setMood(Twinkle);
}


//-------------------------------------------------------------------------
void loop() { 

  static uint32_t lastRenderTime = 0;

  // record which input is currently active
  static const byte NO_INPUT = 255;
  static byte activeInput = NO_INPUT;
  static bool scoreChanged = false;

  // make sure RNG is as random as possible
  random16_add_entropy(random());

  // update animation and check inputs every FRAME_TIME ms
  if (millis() - lastRenderTime > FRAME_TIME) {
    lastRenderTime = millis();

    // check inputs
    activeInput = NO_INPUT;
    for (int i=0; activeInput == NO_INPUT && i< NUM_LEDS; i++) {
      if (digitalRead(INPUT_PINS[i]) == LOW) {
        activeInput = i;
      }
    }

    // if an input active, switch to MoodSelected
    if (activeInput != NO_INPUT) {
      setMood(Selected, activeInput);
      scoreChanged = false;
    }
    else {
      // if Selected completed and score was increased, show Report,
      // otherwise go back to Twinkle
      if (scoreChanged) {
        setMood(Report);
      }
      else {
        setMood(Twinkle);
      }
    }

    bool running = curMood->run();
    copyLedsToRealLeds(curMood->fadeStep());

    if (curMoodID == Selected && activeInput != NO_INPUT && !running) {
      // Selected anim has completed, so add 1 to the score
      // qadd8 is used to clamp max at 255 instead of rolling round
      scores[activeInput] = qadd8(scores[activeInput], 1);

      scoreChanged = true;
    }
    else if (curMoodID == Report && !running) {
      // report has finished. Return to twinkle
      setMood(Twinkle);
      scoreChanged = false;
    }
  }

  // update LEDs
  FastLED.show();
}
