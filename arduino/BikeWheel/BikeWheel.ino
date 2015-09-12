// JCC 13 August 2015
//
// Initial implementation of RGB animation for the HeatHack bicycle wheel, based on the FastLED libraries, but instead
// of driving any kind of LED strip, multiplexing through a set of normal RGB LEDs.

// We're eventually aiming for a two-part firmware structure.  Part A calculates colours in an animation and stores them in colourbuffer. 
// Part B cycles through the LEDs getting the colours from colourbuffer and lighting the LEDs. 
//
// This version has a very preliminary implementation for part A that just chooses base colours randomly at a chosen interval.
// Eventually the colours will be chosen from a set that go with the fabrics on the wheel representing different categories of user input.
// The animation will mill about in some way until there is user input, respond to that, and then mill about until the next time
//
// This version has a sort-of working version of the multiplexer.

//TODO: find out what's up with AliceBlue, why colours are faint, whether will burn out Arduino...
//TODO: write animation (maybe each LED should be a different colour and move smoothly to the next choice).
//TODO: we'll have trouble with times rolling over every 70 minutes unless we use twos complement arithmetic?  I think I've already
// observed this - the animation hangs on one state some times.
//-------------------------------------------------------------------------------------------
// get the FastLED library from http://fastled.io/
#include "FastLED.h" 

//-------------------------------------------------------------------------------------------
// BEGIN CONFIGURATION
// We may need debugging output.  0 for normal, 1 for go slow and use serial debug statements.
#define VERBOSE 0
     
// How many RGB leds?
#define NUM_LEDS 6

// what PWM pins are we using for R, G, and B in the multiplexing?  
// Uno has PWM on 3, 5, 6, 9, 10, 11.  They aren't all the same.
// On an Uno, most PWM pins have a frequency of 490 Hz; 5 and 6 are faster, at 980 Hz. 
// Plus if you change the default PWM frequency, that has implications for other Arduino functions - see below.
#define RED_PIN 9
#define GREEN_PIN 10
#define BLUE_PIN 11

// what are our base colours for our colour scheme?
const CRGB basecolour[NUM_LEDS] = {CRGB::White, CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Cyan};
//const CRGB basecolour[NUM_LEDS] = {CRGB::Green,CRGB::Red,CRGB::Blue,CRGB::Green,CRGB::Red};


// How often should we loop through the animation that makes the LEDs mill about?  In microseconds.
long animationInterval = 30000000;
// when's the last time we did it?
long previousAnimationTime = 0;

// and what pins are we using for the NUM_LEDS grounds on the common cathodes?
const int cathode_pin[NUM_LEDS] = {2,3,4,5,6,7};


// a buffer for the colours that the multiplexer should pick up.
CRGB colourbuffer[NUM_LEDS];
// syntax for use:  colourbuffer[i].red =    50; OR colourbuffer[i] = CRGB( 50, 100, 150);
// OR use html colours like colourbuffer[0] = CRGB::Red;


// and the multiplexing time control.  
// How long are we lighting the LED for? In Microseconds.
long onInterval = 1000; // theoretical maximum best value is 3000, but that might be hardware dependent for some other hardware?
// for me 3000 flickers in peripheral vision.

//long onInterval = 24000; //
long onIntervalForDebugging = 100000; //good value for coping with Serial and seeing colours.

// and at what time did led i last go on (0 if not on). In microseconds.
long onTime[NUM_LEDS] = {0,0,0,0,0};

// In theory, to avoid flicker, we need to:
///   (1) get through the set of LEDs every 17 ms, at least. This value may be hardware dependent on different hardware...
///   (2) synchronize the PWM and the multiplexing so that we don't get too much PWM off time while the LED is supposed to be on -
/// that will cause the light to flicker.  This sounds hard; for 5 LEDs we can afford 3 ms per LED, but the PWM cycle is 1 or 2 ms.
/// We get strong colours with longer onIntervals, but of course the LEDS don't appear to be continuously lit.
/// The "expensive" solution is to buy an LED driver.  
// The cheap solution appears to be changing from the default PWM frequency and being very careful about how that messes
// with the timing functions. http://playground.arduino.cc/Code/PwmFrequency.   If this is on, choose timing functions and PWM pins carefully.
#define CHANGE_PWM 1 //mess with the PWM or not on this attempt? A debugging switch.
int pwmDivisor = 1; // 1, 8, 64, ... as below.
//1 is not the same as without resetting the frequency?  Less flickery...


// how many user input buttons?  
const int NUM_INPUTS = 6;
// and what pins are they? 
const int input_pin[NUM_INPUTS] = {A0,A1,A2,A3,A4,A5};

// END CONFIGURATION
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// BEGIN BORROWED CODE
// From http://playground.arduino.cc/Code/PwmFrequency
/**
 * Divides a given PWM pin frequency by a divisor.
 *
 * The resulting frequency is equal to the base frequency divided by
 * the given divisor:
 *   - Base frequencies:
 *      o The base frequency for pins 3, 9, 10, and 11 is 31250 Hz.
 *      o The base frequency for pins 5 and 6 is 62500 Hz.
 *   - Divisors:
 *      o The divisors available on pins 5, 6, 9 and 10 are: 1, 8, 64,
 *        256, and 1024.
 *      o The divisors available on pins 3 and 11 are: 1, 8, 32, 64,
 *        128, 256, and 1024.
 *
 * PWM frequencies are tied together in pairs of pins. If one in a
 * pair is changed, the other is also changed to match:
 *   - Pins 5 and 6 are paired on timer0
 *   - Pins 9 and 10 are paired on timer1
 *   - Pins 3 and 11 are paired on timer2
 *
 * Note that this function will have side effects on anything else
 * that uses timers:
 *   - Changes on pins 3, 5, 6, or 11 may cause the delay() and
 *     millis() functions to stop working. Other timing-related
 *     functions may also be affected.
 *   - Changes on pins 9 or 10 will cause the Servo library to function
 *     incorrectly.
 *
 * Thanks to macegr of the Arduino forums for his documentation of the
 * PWM frequency divisors. His post can be viewed at:
 *   http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1235060559/0#4
 */
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
//END BORROWED CODE
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
//BEGIN AUXILIARY FUNCTIONS
void debug(String str) {
  if (VERBOSE) {Serial.println(str);};
}

// give each LED a colour randomly chosen from our colour scheme.
/// We may want to force them all to be different.
void initializecolourbuffer() {
  for (int i=0; i< NUM_LEDS; i++) {
     long choice = random(0,NUM_LEDS); 
     //colourbuffer[i] = basecolour[(int)choice];
     colourbuffer[i] = basecolour[i]; //TEMP DEBUG
   }  
}

//int previous(int i) {
//  if (i == 0) {
//    return NUM_LEDS;
//  } else {
//    return (i-1);
//  }
//}


int next(int i) {
  if (i >= (NUM_LEDS - 1)) {
    return 0;
  } else {
    return (i+1);
  }
}
//END AUXILIARY FUNCTIONS
//-------------------------------------------------------------------------------------
//--------------------------------------------------------------------------
// BEGIN PART A
// for now, just pick random base colours for the LEDS. 
void animate() {
  for (int i=0; i< NUM_LEDS; i++) {
     //STUB pick a random base colour
     long choice = random(0,NUM_LEDS);
     colourbuffer[i] = basecolour[(int)choice];
     previousAnimationTime = micros();
   }  
}

// for now, register an input pin by blinking.
void blink() {
   for (int i=0; i< NUM_LEDS; i++) {
      digitalWrite(cathode_pin[i],HIGH);
   }
   delay(1000);
}

//END PART A
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//BEGIN PART B
void turnLedOff(int i) {
  digitalWrite(cathode_pin[i],HIGH);
  debug((String)i + " off at " + micros());
  onTime[i] = 0;
}


void turnLedOn(int i) {
  analogWrite(RED_PIN, colourbuffer[i].red);
  analogWrite(GREEN_PIN, scale8_video(colourbuffer[i].green, 224));
  analogWrite(BLUE_PIN, scale8_video(colourbuffer[i].blue, 140));     
  onTime[i] = micros();
  digitalWrite(cathode_pin[i],LOW);
  debug((String)i + " on at " + (String) onTime[i] + " with red " + (String)colourbuffer[i].red);
}

void lightFirstLed() {
     turnLedOn(0); 
}
  
//END PART B
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// BEGIN MAIN PROGRAM
void setup() { 
     if (VERBOSE) {
       Serial.begin(9600);
       onInterval = onIntervalForDebugging; 
       randomSeed(1); // make random choice behaviour the same every time (once animation is included)
     }
     if (CHANGE_PWM) {
        setPwmFrequency(RED_PIN, pwmDivisor);  
        setPwmFrequency(GREEN_PIN, pwmDivisor);
        setPwmFrequency(BLUE_PIN, pwmDivisor);
     }
     // set all input pins to input and pull everything high.  The wand will be connected to ground and pull
     // the input pin low.
     for (int i=0; i< NUM_INPUTS; i++) {
          pinMode(input_pin[i], INPUT);
          digitalWrite(cathode_pin[i],HIGH);
      } 
      // set all LED pins to output and pull everything high.  We'll set the cathodes low to light an LED.
      for (int i=0; i< NUM_LEDS; i++) {
          pinMode(cathode_pin[i], OUTPUT);
          digitalWrite(cathode_pin[i],HIGH);
      } 
      initializecolourbuffer();
      lightFirstLed();
}

void loop() { 

  // STILL TO ADD:  interrupt for the user inputs.  Maybe like http://playground.arduino.cc/Code/Stopwatch
 
  for (int i=0; i< NUM_LEDS; i++) {
    if ((onTime[i] > 0) and((micros() - onTime[i]) > onInterval)) {
      turnLedOff(i);
      turnLedOn(next(i));
    }
  }

 if ((micros() - previousAnimationTime) > animationInterval) {
    animate();
 }
 
 for (int i=0; i< NUM_INPUTS; i++) {
    if (input_pin[i] == LOW) {
      blink();
    }
  }

}

//END MAIN PROGRAM
//----------------------------------------------------------

