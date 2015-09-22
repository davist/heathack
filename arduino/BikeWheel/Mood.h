
#ifndef __INC_MOOD_H
#define __INC_MOOD_H

class Mood {
public:
  virtual ~Mood() {}

  // compute next frame and write it to the leds array
  virtual bool run() = 0;
  
  // rate to update led colour when copying from leds to realLeds
  virtual uint8_t fadeStep() { return 60; }
};

#endif

