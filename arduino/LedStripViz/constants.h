#ifndef __INC_MP_CONSTANTS_H
#define __INC_MP_CONSTANTS_H

#include "FastLED.h"
extern CRGB leds[];

// type of led strip
#define LED_TYPE WS2812B
#define RGB_ORDER GRB

// strip length
#define NUM_LEDS 60

// pin the led strip's data wire is connected to
#define DATA_PIN 12

// animation speed in frames per second
#define FPS 30
// time per frame in ms
#define FRAME_TIME (1000/FPS)

// time for switching between patterns and running previous one down to black
#define PATTERN_CHANGE_TIME 2000

// a pin connected to an indicator led
#define INDICATOR_LED_PIN 13

// pins for switch
#define PIN_ANIM_SWITCH 4

// dht11 sensor
#define DHT11_PIN 2
#define DHT11_INTERRUPT 0
#define DHT11_WAIT_TIME 2000

// dallas temp sensor
#define DT_PIN 3
#define DT_TEMPERATURE_PRECISION 9
// conversion time extracted from DallasTemperature.cpp to avoid calling a fn to get a constant
#define DT_CONVERSION_TIME 94
#define REQUIRESALARMS false

// changeover point from hot to cold patterns
#define MIN_HOT_TEMP 16

#endif
