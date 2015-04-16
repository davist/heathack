#include <FastLED.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SevSeg.h>
#include <TimerOne.h>

#include "Pattern.h"
#include "PatternFire2012.h"
#include "PatternPrecipitation.h"

#include "constants.h"

#define DEBUG false


// Define the array of leds
CRGB leds[NUM_LEDS];

Pattern* curPattern;
uint8_t nextPattern;
uint32_t nextPatternStartTime;

void prepareNextPattern(PatternType type);
void changePattern(void);

// temp sensor
OneWire oneWire;

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature tempSensor(&oneWire);
DeviceAddress tempDeviceAddress;
bool isTempDeviceAvailable = false;

// 7-segment display
SevSeg tempDisplay;
volatile char tempDigits[3];

/////////////////////////////////////////


int16_t getReading(void) {

  static bool acquiringReading = false;
  static uint32_t lastReadTime = 0;
  int16_t reading = NO_READING;
  static int16_t lastSimReading = MIN_HOT_TEMP;
  static int8_t simDirection = 1;
  
  if (isTempDeviceAvailable) {
    // read temperature
    
    if (!acquiringReading) {
      tempSensor.requestTemperatures();    
      lastReadTime = millis();
      acquiringReading = true;
    }
    else if (millis() - lastReadTime > DT_CONVERSION_TIME) {
    
      int16_t rawReading = tempSensor.getTemp(tempDeviceAddress);

      if (rawReading != DEVICE_DISCONNECTED_RAW) {
        // Raw reading is 15 bits - 8 for whole degrees plus 7 for fraction of a degree.
        // As we set precision to 9 there will only ever be one bit used for the fraction
        // which represents 0.0 or 0.5 of a degree.
        // reading is is 1/10s of a degree (i.e. 10 times actual value so 21.5 is represented as 215).
        reading = ((rawReading >> 7) * 10) + ((rawReading & 0x7F) ? 5 : 0);
        acquiringReading = false;
#if DEBUG
        Serial.print("temp = ");
        Serial.println(reading);
        Serial.flush();
      }
      else {        
        Serial.println("Temp sensor disconnected");
        Serial.flush();
#endif
      }
    }
  }
  else {
    // simulate temp rising and falling
    if (millis() - lastReadTime > SIM_STEP_TIME) {
      lastReadTime = millis();
      lastSimReading += simDirection;
      if (lastSimReading > MAX_SIM_READING || lastSimReading < MIN_SIM_READING) {
        simDirection *= -1;
        lastSimReading += simDirection;
      }
      reading = lastSimReading;
    }
  }
  
  return reading;  
}

  
byte getIntensity(int16_t reading) {

  // calc intensity
  byte intensity;

  if (nextPattern) {
    // run down to black
    intensity = 0;
  }
  else {
    if (reading < MIN_HOT_TEMP) {
      // cold - intensity increases as temp falls from 15 to 5
      intensity = 10 - qsub8(reading > 0 ? reading/10 : 0, 5);
    }
    else {
      // hot - intensity increases as temp goes from 16 to 36
      intensity = (reading - MIN_HOT_TEMP) / 20;
    }
    
    // want 1 - 11 instead of old 0 - 10 so 0 can be for "run down"
    intensity++;
  }
  
  if (intensity > 11) intensity = 11;

  return intensity;
}


void prepareNextPattern(PatternType type) {
  nextPattern = type;
  nextPatternStartTime = millis() + PATTERN_CHANGE_TIME;
}


void changePattern(void) {

  if (curPattern) delete curPattern;

  switch (nextPattern) {

    case HOT_PATTERN:
      curPattern = new PatternFire2012();
      break;
    case COLD_PATTERN:
      curPattern = new PatternPrecipitation(&PrecipSnow);
      break;
  }

  nextPattern = UNDEFINED;
  FastLED.clear();
}

// interrupt routine for updating 7-seg display
void updateTempDisplay(void) {
  tempDisplay.DisplayString((char *)tempDigits, 0);
}

void setup() {

  #if DEBUG
  Serial.begin(57600);
  Serial.println("**************************************************");
  #endif
  
  // initialise FastLED
  FastLED.addLeds<LED_TYPE, DATA_PIN, RGB_ORDER>(leds, NUM_LEDS);
  // clear down any power-on led randomness asap
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setDither(DISABLE_DITHER);
  FastLED.show();

  // initialise Dallas Temp
  oneWire.init(DT_PIN);
  tempSensor.begin();
  tempSensor.setWaitForConversion(false);

  // Grab a count of devices on the wire
  if (tempSensor.getDeviceCount() > 0 && tempSensor.getAddress(tempDeviceAddress, 0)) {
    isTempDeviceAvailable = true;
    tempSensor.setResolution(tempDeviceAddress, DT_TEMPERATURE_PRECISION);
  }

  if (isTempDeviceAvailable){
    // initialise temperature display
    tempDisplay.Begin(TD_TYPE, TD_DIGITS, TD_DIGIT1_PIN, TD_DIGIT2_PIN, 0, 0,
      TD_SEG_A_PIN, TD_SEG_B_PIN, TD_SEG_C_PIN, TD_SEG_D_PIN, TD_SEG_E_PIN, TD_SEG_F_PIN, TD_SEG_G_PIN, 0);
      
    tempDisplay.SetBrightness(100);
    
    // update display every 20ms
    Timer1.initialize(20000);
    Timer1.attachInterrupt(updateTempDisplay);
  }
  #if DEBUG
  else {
    Serial.println("No temperature sensor connected");
    Serial.flush();
  }
  #endif
  
  // initialise pattern
  prepareNextPattern(HOT_PATTERN);
  changePattern();
}


void loop() {
  unsigned long startTime = millis();

  // reading is 10x actual integer value to allow for 1 decimal place
  int16_t reading=210;
  static uint16_t lastReading=210;
  static uint8_t globalBrightness = 255;
  
  reading = getReading();
  
  if (reading == NO_READING) {
    reading = lastReading;
  }
  
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random());
  
  // reading changed. has it gone from hot to cold or vice versa?
  if (reading < MIN_HOT_TEMP && lastReading >= MIN_HOT_TEMP) {
    // gone from hot to cold
    prepareNextPattern(COLD_PATTERN);
  }
  else if (reading >= MIN_HOT_TEMP && lastReading < MIN_HOT_TEMP) {
    // gone from cold to hot
    prepareNextPattern(HOT_PATTERN);
  }

  lastReading = reading;

  if (isTempDeviceAvailable){
    cli();
    sprintf((char *)tempDigits, "%2d", reading / 10);
    sei();
  }
  
  if (nextPattern) {
    if (millis() > nextPatternStartTime) {
      changePattern();
      globalBrightness = 255;
    }
    else {
      globalBrightness = qsub8(globalBrightness, GLOBAL_FADE_STEP);
    }
  }

  if (curPattern) {
    curPattern->run(getIntensity(reading));
  }

  // Want to start each frame after FRAME_TIME ms since last frame instead of
  // having a fixed delay between frames.
  // Time to render each frame could vary.

  uint32_t renderTime = millis() - startTime;  
  delay(FRAME_TIME - renderTime);
  
  FastLED.show(globalBrightness);
}

