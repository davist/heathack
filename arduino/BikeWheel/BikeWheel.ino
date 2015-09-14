#include <FastLED.h>
#include "NonProgrammableLEDController.h"

//-------------------------------------------------------------------------------------------
// BEGIN CONFIGURATION
// We may need debugging output.  false for normal, true for go slow and use serial debug statements.
#define DEBUG false
     
// How many RGB LEDs (and inputs)?
#define NUM_LEDS 6

// what PWM pins are we using for R, G, and B in the multiplexing?  
// Uno has PWM on 3, 5, 6, 9, 10, 11.  They aren't all the same.
// On an Uno, most PWM pins have a frequency of 490 Hz; 5 and 6 are faster, at 980 Hz. 
// Plus if you change the default PWM frequency, that has implications for other Arduino functions - see below.
#define RED_PIN 9
#define GREEN_PIN 10
#define BLUE_PIN 11

// frames per sec for animation
#define FPS 3

// what are our base colours for our colour scheme?
const CRGB basecolour[NUM_LEDS] = {CRGB::White, CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Cyan};

// How often should we loop through the animation that makes the LEDs mill about?
unsigned long frameIntervalMs = 1000 / FPS;
// when's the last time we did it?
unsigned long frameStartTime;

// and what pins are we using for the NUM_LEDS grounds on the common cathodes?
const byte cathode_pins[NUM_LEDS] = {2, 3, 4, 5, 6, 7};

// FastLED controller for simple, non-programmable LEDs
NonProgrammableLEDController controller(
    RED_PIN, GREEN_PIN, BLUE_PIN,
    cathode_pins,
    NUM_LEDS
#if DEBUG  // slow down multiplexing
    , true
    , 0
    , 1000000
#endif
);

// a buffer for the colours that the multiplexer should pick up.
CRGB colourbuffer[NUM_LEDS];

const int input_pin[NUM_LEDS] = {A0, A1, A2, A3, A4, A5};


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
     colourbuffer[i] = basecolour[(int)choice];
   }  
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
      pinMode(input_pin[i], INPUT_PULLUP);
    }

     FastLED.addLeds(&controller, colourbuffer, NUM_LEDS);
     FastLED.setDither(DISABLE_DITHER);
     FastLED.setCorrection(Typical8mmPixel);

     frameStartTime = millis();
}


//-------------------------------------------------------------------------
void loop() { 

  // record which input is currently active
  static const byte NO_INPUT = 255;
  static byte activeInput = NO_INPUT;

  // update animation and check inputs every frameIntervalMs
  if (millis() - frameStartTime > frameIntervalMs) {
    frameStartTime = millis();

    // check inputs
    activeInput = NO_INPUT;
    for (int i=0; activeInput == NO_INPUT && i< NUM_LEDS; i++) {
      if (digitalRead(input_pin[i]) == LOW) {
        activeInput = i;
      }
    }

    // if no inputs active, update animation
    if (activeInput == NO_INPUT) animate();
  }

  if (activeInput != NO_INPUT) {
    FastLED.showColor(basecolour[activeInput]);
  }
  else {
    // if no input active, show animation
    FastLED.show();
  }
}
