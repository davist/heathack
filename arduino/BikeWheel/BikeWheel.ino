#include <FastLED.h>
#include "config.h"
#include "NonProgrammableLEDController.h"
#include "Mood.h"
#include "MoodTwinkle.h"
#include "MoodSelected.h"

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



//-------------------------------------------------------------------------
void debug(String str) {
  if (DEBUG) {Serial.println(str);};
}


//-------------------------------------------------------------------------
// for now, just pick random base colours for the LEDS. 
void animate() {
  for (int i=0; i< NUM_LEDS; i++) {
     //STUB pick a random base colour
     long choice = random(0,NUM_LEDS);
     leds[i] = basecolour[(int)choice];
   }  
}


//-------------------------------------------------------------------------
inline void copyLedsToRealLeds(uint8_t step) {
  // copy leds to realLeds with hysteresis
  // bias the colour towards red as it dims

  for (uint8_t i=0; i<NUM_LEDS; i++) {
    CRGB *current = &realLeds[i];
    CRGB *target = &leds[i];

    // red
    if (target->r != current->r) {
      if (target->r > current->r) {
        if (target->r - current->r > step) current->r += step + 6;
        else current->r = target->r;
      }
      else {
        if (current->r - target->r > step) current->r -= step - 6;
        else current->r = target->r;
      }
    }

    // green
    if (target->g != current->g) {
      if (target->g > current->g) {
        if (target->g - current->g > step) current->g += step + 3;
        else current->g = target->g;
      }
      else {
        if (current->g - target->g > step) current->g -= step - 3;
        else current->g = target->g;
      }
    }

    // blue
    if (target->b != current->b) {
      if (target->b > current->b) {
        if (target->b - current->b > step) current->b += step;
        else current->b = target->b;
      }
      else {
        if (current->b - target->b > step) current->b -= step;
        else current->b = target->b;
      }
    }
  }
}


//-------------------------------------------------------------------------
void setMood(MoodID id, uint8_t index = 0) {

  // only change mood if requested one is different to current one
  if (curMoodID == id) return;

  delete curMood;

  switch (id) {
  case Twinkle:
    curMood = new MoodTwinkle();
    break;
  case Selected: {
    uint8_t paletteSize = palettes[PALETTE_RAINBOW].size;
    curMood = new MoodSelected(palettes[PALETTE_RAINBOW].colours[index % paletteSize]);
    break;
  }
  case Report:
    // TODO
    break;
  case None:
    break;
  }

  curMoodID = id;
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
    }
    else {
      setMood(Twinkle);
    }

    curMood->run();
    copyLedsToRealLeds(curMood->fadeStep());
  }

  // update LEDs
  FastLED.show();
}
