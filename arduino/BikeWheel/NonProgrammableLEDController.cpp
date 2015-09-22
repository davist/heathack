/*
 * NonProgrammableLEDController.cpp
 *
 *  Created on: 13 Sep 2015
 *      Author: tim
 */

#include "NonProgrammableLEDController.h"

/**
 * redPin, greenPin, bluePin - pin numbers connected to the r, g, b on all LEDs. These must be PWM pins
 * commonPins - array of pin numbers for the common pin on each LED
 * numLEDs - number of entries in commonPins array
 * commonCathode - true if the common pin is a cathode, false if an anode
 * pwmDivisor - set to 0 to disable PWM modification
 *   The divisors available on pins 5, 6, 9 and 10 are: 1, 8, 64, 256, and 1024
 *   on pins 3 and 11 are: 1, 8, 32, 64, 128, 256, and 1024
 */
NonProgrammableLEDController::NonProgrammableLEDController(byte redPin, byte greenPin, byte bluePin,
    const byte* commonPins, byte numLEDs, bool commonCathode, byte pwmDivisor, unsigned long intervalMicros) :

  redPin(redPin), greenPin(greenPin), bluePin(bluePin),
  commonPins(commonPins), numLEDs(numLEDs), commonCathode(commonCathode),
  pwmDivisor(pwmDivisor), intervalMicros(intervalMicros)
{
  currentLED = 0;
  currentLEDStartTime = 0;
}

void NonProgrammableLEDController::init() {
  // In theory, to avoid flicker, we need to:
  ///   (1) get through the set of LEDs every 17 ms, at least. This value may be hardware dependent on different hardware...
  ///   (2) synchronize the PWM and the multiplexing so that we don't get too much PWM off time while the LED is supposed to be on -
  /// that will cause the light to flicker.  This sounds hard; for 5 LEDs we can afford 3 ms per LED, but the PWM cycle is 1 or 2 ms.
  /// We get strong colours with longer onIntervals, but of course the LEDS don't appear to be continuously lit.
  /// The "expensive" solution is to buy an LED driver.
  // The cheap solution appears to be changing from the default PWM frequency and being very careful about how that messes
  // with the timing functions. http://playground.arduino.cc/Code/PwmFrequency.   If this is on, choose timing functions and PWM pins carefully.

  if (pwmDivisor != 0) {
    setPwmFrequency(redPin, pwmDivisor);
    setPwmFrequency(greenPin, pwmDivisor);
    setPwmFrequency(bluePin, pwmDivisor);
  }

  // set all LED pins to output and turn everything off.
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  setColour(zeros);

  for (byte i=0; i< numLEDs; i++) {
    pinMode(commonPins[i], OUTPUT);
    ledOff(i);
  }
}

void NonProgrammableLEDController::clearLeds(int nLeds) {
  // can't clear some LEDs and not others, so this will always clear all of them
  setColour(zeros);
}

// set all the leds on the controller to a given color
void NonProgrammableLEDController::showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
  updateCurrentLED(&rgbdata, nLeds, scale, false);
}

void NonProgrammableLEDController::show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
  updateCurrentLED(rgbdata, nLeds, scale, true);
}

#ifdef SUPPORT_ARGB
void NonProgrammableLEDController::show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
  updateCurrentLED(rgbdata, nLeds, scale, true);
}
#endif

void NonProgrammableLEDController::updateCurrentLED(const struct CRGB *rgbdata, int inUseLEDs, CRGB scale, bool advance) {

  // time to move onto next LED?
  if (micros() - currentLEDStartTime > intervalMicros) {

    if (inUseLEDs > numLEDs) inUseLEDs = numLEDs;

    ledOff(currentLED);

    currentLED = (currentLED + 1) % inUseLEDs;
    currentLEDStartTime = micros();

    setColour(rgbdata[(advance ? currentLED : 0)], scale);

    ledOn(currentLED);
  }
}

void NonProgrammableLEDController::setColour(const struct CRGB& rgb, CRGB scale) {

  // shortcut for black - ensure LEDs reaaly are off and not just very dim as analogWrite(pin,0) would give you
  if (((uint32_t)rgb.raw) == 0) {
    digitalWrite(redPin, commonCathode ? LOW : HIGH);
    digitalWrite(greenPin, commonCathode ? LOW : HIGH);
    digitalWrite(bluePin, commonCathode ? LOW : HIGH);
  }

  byte scaledValue;
  scaledValue = scale8_video(rgb.r, scale.r);
  analogWrite(redPin, commonCathode ? scaledValue : 255 - scaledValue);

  scaledValue = scale8_video(rgb.g, scale.g);
  analogWrite(greenPin, commonCathode ? scaledValue : 255 - scaledValue);

  scaledValue = scale8_video(rgb.b, scale.b);
  analogWrite(bluePin, commonCathode ? scaledValue : 255 - scaledValue);
}

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
void NonProgrammableLEDController::setPwmFrequency(byte pin, int divisor) {
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
      TCCR0B = (TCCR0B & 0b11111000) | mode;
    } else {
      TCCR1B = (TCCR1B & 0b11111000) | mode;
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
    TCCR2B = (TCCR2B & 0b11111000) | mode;
  }
}

const CRGB NonProgrammableLEDController::zeros(0,0,0);
