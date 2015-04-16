#include <FastLED.h>
#include <idDHT11.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <JeeLib.h>
#include <HeatHack.h>
#include <TimerOne.h>

#include "Pattern.h"
#include "PatternFire2012.h"
#include "PatternPrecipitation.h"
#include "Gauge.h"

#include "constants.h"

enum PatternType { UNDEFINED = 0, HOT_PATTERN = 1, COLD_PATTERN = 2, WET_PATTERN = 3, TEMP_GAUGE = 4, HUM_GAUGE = 5 };

// Define the array of leds
CRGB leds[NUM_LEDS];

Pattern* curPattern;
uint8_t nextPattern;
uint32_t nextPatternStartTime;

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
bool showingPattern = true;

int16_t remoteTemperature = 210;
int16_t remoteHumidity = 500;


/////////////////////////////////////////

void setup() { 

  Serial.begin(57600);
  Serial.println("**************************************************");
  Serial.println("**************************************************");

  // initialise FastLED
  FastLED.addLeds<LED_TYPE, DATA_PIN, RGB_ORDER>(leds, NUM_LEDS);
  // clear down any power-on led randomness asap
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setDither(DISABLE_DITHER);
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
  Timer1.initialize();

  // initialise leds
  buttons.ledOn(isLocalSensors ? LED_LOCAL : LED_REMOTE);
  
  // initialise pattern
  prepareNextPattern(HOT_PATTERN);
  changePattern();
  
  Serial.print("free ram ");
  Serial.println(freeRam());
}

void checkButtons(void) {
  
  static byte displayState = 0;
  
  byte pushed = buttons.pushed();
  if (pushed) {
      if (pushed & BUTTON_CHANGE_SENSORS) {
        isLocalSensors = !isLocalSensors;
        buttons.ledOff(LED_LOCAL | LED_REMOTE);
        buttons.ledOn(isLocalSensors ? LED_LOCAL : LED_REMOTE);
        //startStopRadio();
      }
      if (pushed & BUTTON_CHANGE_PATTERN) {
        displayState = (displayState + 1) & 3;

        switch (displayState) {
          case 0:
            prepareNextPattern(HOT_PATTERN);
            break;
          case 1:
            prepareNextPattern(WET_PATTERN);
            break;
          case 2:
            prepareNextPattern(TEMP_GAUGE);
            break;
          case 3:
            prepareNextPattern(HUM_GAUGE);
            // switch immediately from temp to humidity gauges so that the level
            // doesn't bounce up and down a lot
            changePattern();
            break;
        }
        
      }
  }
}

inline int16_t getReading(void) {
  if (isLocalSensors) {
    // read local attached sensor
    return getLocalReading();
  }
  else {
    // get reading from radio
    return getRemoteReading();
  }
}


int16_t getLocalReading(void) {

  static bool acquiringReading = false;
  static uint32_t lastReadTime = 0;  
  static bool lastUsingTemp = true;  
  int16_t reading = NO_READING;
  
  if (usingTemp != lastUsingTemp) {
    // restart acquisition if switched between temp and humidity
    acquiringReading = false;
    lastUsingTemp = usingTemp;
  }
  
  if (usingTemp && isTempDeviceAvailable) {
    // read temperature
    
    if (!acquiringReading) {
      tempSensor.requestTemperatures();    
      lastReadTime = millis();
      acquiringReading = true;
    }
    else if (millis() - lastReadTime > DT_CONVERSION_TIME) {
    
      reading = (int16_t)(tempSensor.getTempC(tempDeviceAddress) * 10);
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
        reading = (int16_t)(humiditySensor.getHumidity() * 10);
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


int16_t getRemoteReading(void) {
  int16_t reading;
  
  if (usingTemp) {
    reading = remoteTemperature;
  }
  else {
    reading = remoteHumidity;
  }
  
  if (usingTemp) {
    Serial.print("remote temp = ");
  }
  else {
    Serial.print("remote humidity = ");
  }  
  Serial.println(reading);

  return reading;
}

void checkRadio(void) {
  if (rf12_recvDone() &&
      rf12_crc == 0   &&
      (rf12_hdr & RF12_HDR_MASK) == RADIO_REMOTE_NODE_ID) {

    // valid data received

    HeatHackData *data = (HeatHackData *)rf12_data;

    Serial.print(data->numReadings);
    Serial.println(" readings");
            
      for (byte i=0; i<data->numReadings; i++) {
        
        switch (data->readings[i].sensorType) {
          
          case HHSensorType::TEMPERATURE:
            Serial.println("got temp");
            remoteTemperature = data->readings[i].reading;
            break;
            
          case HHSensorType::HUMIDITY:
            Serial.println("got h");
            remoteHumidity = data->readings[i].reading;
            break;            
        }
      }
  }
}
  
byte getIntensity(int16_t reading) {

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
        intensity = 10 - qsub8(reading > 0 ? reading/10 : 0, 5); 
      }
      else {
        // hot - intensity increases as temp goes from 16 to 36
        intensity = (reading - MIN_HOT_TEMP) / 20;
      }
    }
    else {
      // wet - intensity increases from %50 to 90%
     intensity = (reading > 500 ? (reading - 500)/10 : 0) / 4;
    }
    
    // want 1 - 11 instead of old 0 - 10 so 0 can be for "run down"
    intensity++;
  }
  
  if (intensity > 11) intensity = 11;

  return intensity;
}

uint16_t getGaugeLevel(int16_t reading) {

  if (nextPattern) return 0;

  if (usingTemp) {
    // 0 to 50C -> 0 to 500
    if (reading < 0) return 0;
    else if (reading > 500) return 500;
    return reading;
  }
  else {
    // 0 to 100% -> 0 to 1000
    if (reading < 0) return 0;
    else if (reading > 1000) return 1000;
    return reading;
  }
}

void loop() {
  unsigned long startTime = millis();

  // reading is 10x actual integer value to allow for 1 decimal place
  static int16_t reading=210, lastReading=210;
  static uint8_t globalBrightness = 255;
  
  static Gauge gauge;
  
  checkButtons();
  
  reading = getReading();
  
  if (reading == NO_READING) {
    reading = lastReading;
  }
  
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random());
  
  if (usingTemp && showingPattern && !nextPattern && reading != lastReading) {
          
    // reading changed. has it gone from hot to cold or vice versa?
    if (reading < MIN_HOT_TEMP && lastReading >= MIN_HOT_TEMP) {
      // gone from hot to cold
      prepareNextPattern(COLD_PATTERN);
    }
    else if (reading >= MIN_HOT_TEMP && lastReading < MIN_HOT_TEMP) {
      // gone from cold to hot
      prepareNextPattern(HOT_PATTERN);
    }    
  }  

  lastReading = reading;
  
  if (nextPattern) {
    if (millis() > nextPatternStartTime) {
      changePattern();
      globalBrightness = 255;
    }
    else {
      globalBrightness = qsub8(globalBrightness, GLOBAL_FADE_STEP);
    }
  }

  if (showingPattern && curPattern) { 
    curPattern->run(getIntensity(reading));
  }
  else {
    gauge.run(getGaugeLevel(reading), usingTemp);
  }

  // Want to start each frame after FRAME_TIME ms since last frame instead of
  // having a fixed delay between frames.
  // Time to render each frame could vary.

  uint32_t renderTime = millis() - startTime;
  
//  Serial.print("render time ");
//  Serial.println(renderTime);  
  
  byte cycles = renderTime < FRAME_TIME ? FRAME_TIME - renderTime : MIN_RADIO_CYCLES;

  for (;cycles > 0; cycles--) {
    if (!isLocalSensors) checkRadio();    
    delay(1);
  }
    
  FastLED.show(globalBrightness);
}

void prepareNextPattern(PatternType type) {
  nextPattern = type;
  nextPatternStartTime = millis() + PATTERN_CHANGE_TIME;
}

void changePattern() {

  Serial.println("changePattern");
  Serial.flush();
    
  if (curPattern) delete curPattern;
  
  switch (nextPattern) {

    case HOT_PATTERN:
      usingTemp = true;
      showingPattern = true;
      curPattern = new PatternFire2012();
      break;
    case COLD_PATTERN:
      usingTemp = true;
      showingPattern = true;
      curPattern = new PatternPrecipitation(&PrecipSnow);
      break;
    case WET_PATTERN:
      usingTemp = false;
      showingPattern = true;
      curPattern = new PatternPrecipitation(&PrecipRain);
      break;
    case TEMP_GAUGE:
      usingTemp = true;
      showingPattern = false;
      curPattern = NULL;
      break;
    case HUM_GAUGE:
      usingTemp = false;
      showingPattern = false;
      curPattern = NULL;
      break;
  }

  Serial.print("change ptn: free ram ");
  Serial.println(freeRam());
  Serial.flush();

  nextPattern = UNDEFINED;
  FastLED.clear();
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

