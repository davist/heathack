#ifndef __INC_MP_CONSTANTS_H
#define __INC_MP_CONSTANTS_H

#include "FastLED.h"
#include "SevSeg.h"

extern CRGB leds[];

// type of led strip
#define LED_TYPE WS2812B
#define RGB_ORDER GRB

// strip length
#define NUM_LEDS 60

// pin the led strip's data wire is connected to
#define DATA_PIN 5

// animation speed in frames per second
#define FPS 30
// time per frame in ms
#define FRAME_TIME (1000/(FPS))

// time for switching between patterns and running previous one down to black
#define PATTERN_CHANGE_TIME 2000

#define GLOBAL_FADE_STEP 5
//((256 / ((PATTERN_CHANGE_TIME) / (FRAME_TIME)) ) * 2)

// dallas temp sensor
#define DT_PIN 4
#define DT_TEMPERATURE_PRECISION 9
// conversion time extracted from DallasTemperature.cpp to avoid calling a fn to get a constant
#define DT_CONVERSION_TIME 94
#define REQUIRESALARMS false

// changeover point from hot to cold patterns
#define MIN_HOT_TEMP 160

// value returned by getReading if one wasn't available
#define NO_READING -32767

#define SIM_STEP_TIME 300
#define MAX_SIM_READING 350
#define MIN_SIM_READING 50

// 7-segment temperature display
#define TD_TYPE COMMON_CATHODE
#define TD_DIGITS 2
#define TD_DIGIT1_PIN 11
#define TD_DIGIT2_PIN 12
#define TD_SEG_A_PIN A1
#define TD_SEG_B_PIN 13
#define TD_SEG_C_PIN A3
#define TD_SEG_D_PIN A5
#define TD_SEG_E_PIN A4
#define TD_SEG_F_PIN A2
#define TD_SEG_G_PIN A0

enum PatternType { UNDEFINED = 0, HOT_PATTERN = 1, COLD_PATTERN = 2 };

#endif
