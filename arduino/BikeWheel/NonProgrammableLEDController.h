/*
 * NonProgrammableLEDController.h
 *
 *  Created on: 13 Sep 2015
 *      Author: tim
 */

#ifndef NONPROGRAMMABLELEDCONTROLLER_H_
#define NONPROGRAMMABLELEDCONTROLLER_H_

#include "FastLED.h"

class NonProgrammableLEDController : public CLEDController {

private:
  byte redPin, greenPin, bluePin;
  const byte* commonPins;
  byte numLEDs;
  bool commonCathode;
  byte pwmDivisor;
  unsigned long intervalMicros;

  // keep track of curent LED in the round-robin multiplexing as only one LED can be lit at a time
  byte currentLED;

  // time that current LED was turned on
  unsigned long currentLEDStartTime;

  static const CRGB zeros;

  void setPwmFrequency(byte pin, int divisor);

  void inline ledOn(byte num) {
    digitalWrite(commonPins[num], commonCathode ? LOW : HIGH);
  }

  void inline ledOff(byte num) {
    digitalWrite(commonPins[num], commonCathode ? HIGH : LOW);
  }

  void updateCurrentLED(const struct CRGB *rgbdata, int inUseLEDs, CRGB scale, bool advance);

  void setColour(const struct CRGB& rgb, CRGB scale = 0xffffff);

public:
  /**
   * redPin, greenPin, bluePin - pin numbers connected to the r, g, b on all LEDs. These must be PWM pins
   * commonPins - array of pin numbers for the common pin on each LED
   * numLEDs - number of entries in commonPins array
   * commonCathode - true if the common pin is a cathode, false if an anode
   * pwmDivisor - set to 0 to disable PWM modification
   *   The divisors available on pins 5, 6, 9 and 10 are: 1, 8, 64, 256, and 1024
   *   on pins 3 and 11 are: 1, 8, 32, 64, 128, 256, and 1024
   * intervalMicros - number of microseconds to light each LED for
   */
  NonProgrammableLEDController(byte redPin, byte greenPin, byte bluePin,
      const byte* commonPins, byte numLEDs, bool commonCathode = true,
      byte pwmDivisor = 1, unsigned long intervalMicros = 2000);

  virtual void init();

  virtual void clearLeds(int nLeds);

  // set all the leds on the controller to a given color
  virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale);

  virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale);

#ifdef SUPPORT_ARGB
  virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale);
#endif
};

#endif /* NONPROGRAMMABLELEDCONTROLLER_H_ */

