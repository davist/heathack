#ifndef __INC_MP_CONSTANTS_H
#define __INC_MP_CONSTANTS_H

#include <HeatHack.h>
#include "FastLED.h"
extern CRGB leds[];

// type of led strip
#define LED_TYPE WS2812B
#define RGB_ORDER GRB

// strip length
#define NUM_LEDS 60

// pin the led strip's data wire is connected to
#define DATA_PIN PORT1_DIO

// animation speed in frames per second
#define FPS 30
// time per frame in ms
#define FRAME_TIME (1000/FPS)

// time for switching between patterns and running previous one down to black
#define PATTERN_CHANGE_TIME 2000

// buttons and leds
#define BLINK_PLUG_PORT 4
#define LED_LOCAL 1
#define LED_REMOTE 2
#define BUTTON_CHANGE_SENSORS 1
#define BUTTON_CHANGE_PATTERN 2

// dht11 sensor
#define DHT11_PIN PORT_IRQ_AS_DIO
#define DHT11_INTERRUPT PORT_IRQ
#define DHT11_WAIT_TIME 2000

// dallas temp sensor
#define DT_PIN PORT2_DIO
#define DT_TEMPERATURE_PRECISION 9
// conversion time extracted from DallasTemperature.cpp to avoid calling a fn to get a constant
#define DT_CONVERSION_TIME 94
#define REQUIRESALARMS false

// changeover point from hot to cold patterns
#define MIN_HOT_TEMP 16

// value returned by getReading if one wasn't available
#define NO_READING -32767

//jeenode radio
#define RADIO_GROUP_ID 212
#define RADIO_OUR_NODE_ID 1            // we only listen, so not important
#define RADIO_REMOTE_NODE_ID 15        // node we're listening to
#define RADIO_POLL_INTERVAL_US 1000    // check every 1 ms
#define MIN_RADIO_CYCLES 10

#endif
