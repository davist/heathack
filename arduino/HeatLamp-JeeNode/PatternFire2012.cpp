#include "PatternFire2012.h"
#include "FastLED.h"

// Adapted from http://pastebin.com/xYEpxqgq

// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100
//
// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.

PatternFire2012::PatternFire2012() {
  
  intensity = 255;
  
//  this->heat = (uint8_t *)malloc(NUM_LEDS * sizeof(uint8_t));
}

PatternFire2012::~PatternFire2012() {
/*
  if (this->heat) {
    delete(this->heat);
  }
*/  
}

void PatternFire2012::setIntensity(byte intensity) {
  if (intensity != this->intensity) {
    this->intensity = intensity;
    
    if (intensity == 0) {
      // run flame down to nothing
      sparking = 0;
      maxCooling = 10;
      maxHotCooling = 5;
    }
    else {
      intensity--;
      cooling = (10 - intensity) * 7 + 30;         // range 100 to 30
      hotCooling = ((10 - intensity) * 3) / 2 + 20;  // range 35 to 20
      sparking = intensity * 12 + 72;              // range 72 to 192
      sparkZone = intensity / 2 + 2;               // range 2 to 7
      sparkTempMin = intensity * 15 + 50;          // range 50 to 200
      sparkTempMax = intensity * 19 + 65;          // range 65 to 255
    
      maxCooling = (cooling / (NUM_LEDS / 10)) + 2;
      maxHotCooling = (hotCooling / (NUM_LEDS / 10)) + 2;
    }
  }
}

void PatternFire2012::run(byte intensity) {

    byte i;
  
    setIntensity(intensity);
  
    // Step 1.  Cool down every cell a little
    for( i = 0; i < sparkZone; i++) {
      heat[i] = qsub8(heat[i], random8(0, maxHotCooling));
    }
    for( i = sparkZone; i < NUM_LEDS; i++) {
      heat[i] = qsub8(heat[i], random8(0, maxCooling));
    }
 
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( i= NUM_LEDS - 1; i > 1; i--) {
      heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2] ) / 3;
    }
 
    heat[1] = heat[0];
   
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < sparking ) {
      i = random8(sparkZone);
      byte temp = random8(sparkTempMin, sparkTempMax);
      if (heat[i] < temp) heat[i] = temp;
    }
 
    // Step 4.  Map from heat cells to LED colors
    for( i = 0; i < NUM_LEDS; i++) {
        leds[i] = HeatColor(heat[i]);
    }
}

