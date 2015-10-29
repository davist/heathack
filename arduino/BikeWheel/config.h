#ifndef __INC_CONFIG_H
#define __INC_CONFIG_H

#include <FastLED.h>

// We may need debugging output.  false for normal, true for go slow and use serial debug statements.
#define DEBUG false

// leds
const uint8_t NUM_LEDS = 6;

// what PWM pins are we using for R, G, and B in the multiplexing?
// Uno has PWM on 3, 5, 6, 9, 10, 11.  They aren't all the same.
// On an Uno, most PWM pins have a frequency of 490 Hz; 5 and 6 are faster, at 980 Hz.
// Plus if you change the default PWM frequency, that has implications for other Arduino functions - see below.
const uint8_t RED_PIN = 9;
const uint8_t GREEN_PIN = 10;
const uint8_t BLUE_PIN = 11;

// and what pins are we using for the grounds on the common cathodes?
const uint8_t COMMON_PINS[NUM_LEDS] = {2, 3, 4, 5, 6, 7};

const bool COMMON_CATHODE = true;

// animation speed in frames per second
const uint8_t FPS = 30;
// time per frame in ms
const uint16_t FRAME_TIME = (1000/(FPS));

// pins used for the inputs
const uint8_t INPUT_PINS[NUM_LEDS] = {A0, A1, A5, A4, A2, A3};


// Define the array of leds
extern CRGB leds[NUM_LEDS];

#endif

