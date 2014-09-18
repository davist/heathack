#include <FastLED.h>
#include <idDHT11.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <JeeLib.h>
#include <HeatHack.h>
#include <TimerOne.h>

#include "Pattern.h"
#include "PatternFire2012.h"
#include "PatternSnow.h"

#include "constants.h"

// Define the array of leds
CRGB leds[NUM_LEDS];

Pattern* curPattern;
Pattern* nextPattern;
uint32_t nextPatternStartTime;

enum PatternType { HOT_PATTERN, COLD_PATTERN, WET_PATTERN };
void prepareNextPattern(PatternType type);
void changePattern();

// temp sensor
OneWire oneWire(DT_PIN);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature tempSensor(&oneWire);
DeviceAddress tempDeviceAddress;
bool isTempDeviceAvailable;

// humidity sensor
void dht11_wrapper(); // must be declared before the lib initialization

idDHT11 humiditySensor(DHT11_PIN, DHT11_INTERRUPT, dht11_wrapper);

void dht11_wrapper() {
  humiditySensor.isrCallback();
}

// buttons and leds
BlinkPlug buttons (BLINK_PLUG_PORT);

bool usingTemp = true;
bool isLocalSensors = true;

volatile byte remoteTemperature = 0;
volatile byte remoteHumidity = 0;


/////////////////////////////////////////

void setup() { 

  // initialise FastLED
  FastLED.addLeds<LED_TYPE, DATA_PIN, RGB_ORDER>(leds, NUM_LEDS);
  // clear down any power-on led randomness asap
  FastLED.show();

  // initialise Dallas Temp
  tempSensor.begin();
  tempSensor.setWaitForConversion(false);
  
  // Grab a count of devices on the wire
  int numberOfDevices = tempSensor.getDeviceCount();

  if (tempSensor.getDeviceCount() > 0) {
    if (tempSensor.getAddress(tempDeviceAddress, 0)) {
      isTempDeviceAvailable = true;
      tempSensor.setResolution(tempDeviceAddress, DT_TEMPERATURE_PRECISION);
    }
    else {
      isTempDeviceAvailable = false;      
    }
  }

  // initialise transceiver
  rf12_initialize(RADIO_OUR_NODE_ID, RF12_868MHZ, RADIO_GROUP_ID);  
  Timer1.initialize(POLL_INTERVAL_US);
  Timer1.attachInterrupt(checkRadio);

  // initialise leds
  buttons.ledOn(isLocalSensors ? LED_LOCAL : LED_REMOTE);
  
  // initialise pattern
  prepareNextPattern(HOT_PATTERN);
  changePattern();
  
  Serial.begin(9600);
//  Serial.print("free ram ");
//  Serial.println(freeRam());
}

void checkButtons(void) {
  byte pushed = buttons.pushed();
  if (pushed) {
      if (pushed & BUTTON_CHANGE_SENSORS) {
        isLocalSensors = !isLocalSensors;
        buttons.ledOff(LED_LOCAL | LED_REMOTE);
        buttons.ledOn(isLocalSensors ? LED_LOCAL : LED_REMOTE);
      }
      if (pushed & BUTTON_CHANGE_PATTERN) {
        usingTemp = !usingTemp;
        prepareNextPattern(usingTemp ? HOT_PATTERN : WET_PATTERN);
      }
  }
}


inline byte getReading(void) {
  if (isLocalSensors) {
    // read local attached sensor
    return getLocalReading();
  }
  else {
    // get reading from radio
    return getRemoteReading();
  }
}


byte getLocalReading(void) {

  static bool acquiringReading= false;
  static uint32_t lastReadTime = 0;  

  byte reading;
  
  if (usingTemp && isTempDeviceAvailable) {
    // read temperature
    
    if (!acquiringReading) {
      tempSensor.requestTemperatures();    
      lastReadTime = millis();
      acquiringReading = true;
    }
    else if (tempSensor.isConversionAvailable(tempDeviceAddress) ||
             millis() - lastReadTime > DT_CONVERSION_TIME) {
    
      reading = (byte)tempSensor.getTempC(tempDeviceAddress);
      acquiringReading = false;
      Serial.print("local temp = ");
      Serial.println(reading);
    }
  }
  else if (!usingTemp) {
    // read humidity

    if (!acquiringReading && millis() - lastReadTime > DHT11_WAIT_TIME) {
      // get next reading
      humiditySensor.acquire();
      acquiringReading = true;
    }
    
    if (acquiringReading && !humiditySensor.acquiring()) {
      // result is ready
  
      if (humiditySensor.getStatus() == IDDHTLIB_OK) {
        reading = (byte)humiditySensor.getHumidity();
        Serial.print("local humidity = ");
        Serial.println(reading);
      }
  
      // must wait 2 sec before tying next reading    
      acquiringReading = false;
      lastReadTime = millis();
    }
  }
  
  return reading;  
}


byte getRemoteReading(void) {
  byte reading;
  
  noInterrupts();
  if (usingTemp) {
    reading = remoteTemperature;
  }
  else {
    reading = remoteHumidity;
  }
  interrupts();
  
  return reading;
}

void checkRadio(void) {
  if (rf12_recvDone() &&
      rf12_crc == 0 &&
      (rf12_hdr & RF12_HDR_MASK) == RADIO_REMOTE_NODE_ID) {

    // valid data received
    
    HeatHackData *data = (HeatHackData *)rf12_data;
            
      for (byte i=0; i<data->numReadings; i++) {
        
        switch (data->readings[i].sensorType) {
          
          case HHSensorType::TEMPERATURE:
            remoteTemperature = data->readings[i].reading / 10;
            break;
            
          case HHSensorType::HUMIDITY:
            remoteHumidity = data->readings[i].reading / 10;
            break;            
        }
      }
  }
}
  
byte getIntensity(byte reading) {
    // calc intensity
  byte intensity;

  if (nextPattern) {
    // run down to black
    intensity = 0;
  }
  else {
    if (usingTemp) {  
      if (reading < MIN_HOT_TEMP) {
        // cold - intensity increases as temp falls from 15 to 5
        intensity = 10 - qsub8(reading, 5); 
      }
      else {
        // hot - intensity increases as temp goes from 16 to 36
        intensity = (reading - MIN_HOT_TEMP) / 2;
      }
    }
    else {
      // wet - intensity increases from %60 to 90%
     intensity = qsub8(reading, 60) / 3;
    }
    
    // want 1 - 11 instead of old 0 - 10 so 0 can be for "run down"
    intensity++;
  }
  
  if (intensity > 10) intensity = 10;

  return intensity;
}

void loop() {
  unsigned long startTime = millis();

  static byte reading=21, lastReading=21;
  
  checkButtons();
  
  reading = getReading();
  
  if (reading == NO_READING) {
    reading = lastReading;
  }
  
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random());
  
  if (usingTemp && !nextPattern && reading != lastReading) {
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
  }  
  
  if (nextPattern && millis() > nextPatternStartTime) {
    changePattern();
  }
  
  if (curPattern) { 
    curPattern->run(getIntensity(reading));
  }
  
  // Want to start each frame after FRAME_TIME ms since last frame instead of
  // having a fixed delay between frames.
  // Time to render each frame could vary.
  
  unsigned long renderTime = millis() - startTime;

  if (renderTime < FRAME_TIME) {
    delay(FRAME_TIME - renderTime);
  }

  FastLED.show();
}

void prepareNextPattern(PatternType type) {
    switch (type) {

      case HOT_PATTERN:
        nextPattern = new PatternFire2012();
        break;
      case COLD_PATTERN:
        nextPattern = new PatternSnow();
        break;
      case WET_PATTERN:
//        nextPattern = new PatternRain();
        break;
    }
    
    nextPatternStartTime = millis() + PATTERN_CHANGE_TIME;
}

void changePattern() {
    
    delete curPattern;
    curPattern = nextPattern;
    nextPattern = NULL;
    FastLED.clear();
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

