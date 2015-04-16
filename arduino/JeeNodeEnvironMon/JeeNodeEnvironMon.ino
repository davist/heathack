/**
 * Default setup is:
 *
 * - Room board on ports 2 and 3 with the PIR sensor pointing away from the other ports.
 * With the writing on the room board the right way up, port 2 is on the left and port 3 on the right.
 * This means the HYT131 temp/humidity sensor is on port 2, LDR light sensor on port 3 A pin and
 * PIR motion sensor on port 3 D pin.
 *
 * - A OneWire bus on port 4 data pin supporting multiple DS18B temp sensors.
 * If power is drawn from the A pin instead of +, then the sensors will only be powered
 * when needed to avoid wasting power.
 * Parasitic power mode is supported transparently, though may need a slightly lower value pull-up
 * resistor e.g. 3600 instead of 4700 ohms.
 *
 * - DHT11/22 temp/humidty sensor on port 1 with its data line connected to the D pin. Again, A
 * can be used instead of + to power it only when taking readings.
 *
 * To enable/disable the various sensors depending on your setup, comment/uncomment the lines
 * defining the port for the relevant sensor.
 *
 * If the LCD display is connected and enabled it will be used to display readings.
 */

#define DEBUG true

// dht11/22 temp/humidity
#define DHT_PORT 1
// can't have DHT and PIR both using interrupts
#define DHT_USE_INTERRUPTS false

// Dallas DS18B switchable parasitic power board
#define DS18B_PORT 4

// room node module
//#define HYT131_PORT 2   // the temp/humidity sensor
//#define LDR_PORT    3   // light sensor
//#define PIR_PORT    3   // motion detector

// LCD display
//#define LCD_PORT 1

#include <Arduino.h>
#include "JeeLib.h"
#include "PortsLCD.h"
#include "OneWire.h"

#include "HeatHack.h"
#include "HeatHackSensors.h"
#include "HeatHackShared.h"


#if DHT_PORT
  DHT dht(DHT_PORT);
#endif

#if DS18B_PORT
  DS18B ds18b(DS18B_PORT);
  uint8_t ds18bNumDevices;
#endif

#if HYT131_PORT
    PortI2C hytI2C(HYT131_PORT);
    HYT131 hyt131(hytI2C);
#endif

#if LDR_PORT
    Port ldr(LDR_PORT);
#endif

#if LCD_PORT
  PortI2C lcdI2C(LCD_PORT);
  LiquidCrystalI2C lcd(lcdI2C);
#endif

#if PIR_PORT
    #include <util/atomic.h>

    #define PIR_HOLD_TIME    1  // hold PIR value this many seconds after change
    #define PIR_PULLUP       1   // set to one to pull-up the PIR input pin
    #define PIR_INVERTED     1   // 0 or 1, to match PIR reporting high or low
    #define PIR_STARTUP_SECS 120 // wait this many secs before enabling interrupt to
                                 // avoid spurious triggers while device is stabilising
    
    /// Interface to a Passive Infrared motion sensor.
    class PIR : public Port {
        volatile byte value;
        volatile uint16_t count;
        volatile uint32_t lastOn;
    public:
        PIR (byte portnum)
            : Port (portnum), value (0), count (0), lastOn (0) {}

        // this code is called from the pin-change interrupt handler
        void poll(void) {
            // see http://talk.jeelabs.net/topic/811#post-4734 for PIR_INVERTED
            byte pin = digiRead() ^ PIR_INVERTED;
            // if the pin just went on, then set the changed flag to report it
            if (pin) {
                if (!state()) count++;
                lastOn = millis();
            }
            value = pin;
        }

        // state is true if curr value is still on or if it was on recently
        byte state(void) const {
            byte f = value;
            if (lastOn > 0) {
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                    if (millis() - lastOn < PIR_HOLD_TIME * 1000) {
                        f = 1;
                    }
                }
            }
            
            return f;
        }

        // return number of motion triggers since last time the method was called
        uint16_t triggerCount(void) {
            uint16_t result = count;
            count = 0;
            return result;
        }
        
        void enableInterrupt(void) {
          #if DEBUG
            Serial.println(F("enabling PIR interrupts"));
            serialFlush();
          #endif

          bitSet(PCMSK2, PIR_PORT + 3);
          bitSet(PCICR, PCIE2);
        }
        
        void disableInterrupt(void) {
          #if DEBUG
            Serial.println(F("disabling PIR interrupts"));
            serialFlush();
          #endif

          bitClear(PCICR, PCIE2);
        }
    };

    PIR pir (PIR_PORT);

    // the PIR signal comes in via a pin-change interrupt
    ISR(PCINT2_vect) { pir.poll(); }
#endif


/////////////////////////////////////////////////////////////////////
void doMeasure() {
  static bool firstMeasure = true;
  int humi, temp;
  
  // format data packet
  dataPacket.clear();

  bool lowBatt = rf12_lowbat();
  if (lowBatt || firstMeasure) {
    HHReading reading;
    reading.sensorType = HHSensorType::LOW_BATT;
    reading.encodedReading = lowBatt;
    dataPacket.addReading(reading);
  }

#if DHT_PORT
  dht.reading(dataPacket);
#endif

#if DS18B_PORT
  ds18b.reading(dataPacket);
#endif

#if HYT131_PORT
    hyt131.reading(temp, humi);
    dataPacket.addReading(SENSOR_TEMP, HHSensorType::TEMPERATURE, temp);
    dataPacket.addReading(SENSOR_HUMIDITY, HHSensorType::HUMIDITY, humi);
#endif

#if LDR_PORT
    ldr.digiWrite2(1);  // enable AIO pull-up
    byte light = ~ ldr.anaRead() >> 2;
    ldr.digiWrite2(0);  // disable pull-up to reduce current draw
    
    dataPacket.addReading(SENSOR_LIGHT, HHSensorType::LIGHT, light);
#endif

#if PIR_PORT
    dataPacket.addReading(SENSOR_MOTION, HHSensorType::MOTION, pir.triggerCount());
#endif

  firstMeasure = false;
}


/////////////////////////////////////////////////////////////////////
#if LCD_PORT
void displayReadingsOnLCD(void) {

  lcd.clear();

  // display 1st 4 readings
  for (byte i=0; i<4 && i<dataPacket.numReadings; i++) {
    switch (i) {
      case 0:
        lcd.setCursor(0,0);
        break;
      case 1:
        lcd.setCursor(8,0);
        break;
      case 2:
        lcd.setCursor(0,1);
        break;
      case 3:
        lcd.setCursor(8,1);
        break;
    }

    lcd.print(dataPacket.readings[i].sensorNumber);
    lcd.print(F(":"));
    lcd.print(dataPacket.readings[i].getIntPartOfReading());

    uint8_t decimal = dataPacket.readings[i].getDecPartOfReading();        
    if (decimal != NO_DECIMAL) {
      // display as decimal value to 1 decimal place
      lcd.print(F("."));
      lcd.print(decimal);
    }
    lcd.print(HHSensorUnitNames[dataPacket.readings[i].sensorType]);
  }  
}
#endif

/////////////////////////////////////////////////////////////////////
void setup() {

  // wait for things to stablise
  delay(1000);

  readEeprom();

  uint8_t mins, secs;

  #if LCD_PORT
    lcd.begin(LCD_WIDTH, LCD_HEIGHT);
    lcd.noBacklight();

    mins = myInterval / 6;
    secs = (myInterval % 6) * 10;

    lcd.print(F("HeatHack E-Mon"));
    lcd.setCursor(0,1);
    lcd.print(F("G:"));
    lcd.print(myGroupID);
    lcd.print(F(" N:"));
    lcd.print(myNodeID);
    lcd.print(F(" I:"));
    if (mins != 0) {
      lcd.print(mins);
      lcd.print(F("m"));
    }
    else {
      lcd.print(secs);
      lcd.print(F("s"));
    }      
  #endif
  
  Serial.begin(BAUD_RATE);
  
  Serial.println(F("JeeNode HeatHack Environment Monitor"));

  configConsole();

  Serial.print(F("Using group id "));
  Serial.print(myGroupID);
  Serial.print(F(" and node id "));
  Serial.print(myNodeID);
  Serial.println();
  Serial.print(F("Transmit interval is "));
  mins = myInterval / 6;
  secs = (myInterval % 6) * 10;
  switch (mins) {
    case 0:
      break;
    case 1:
      Serial.print(F("1 minute"));
      break;
    default:
      Serial.print(mins);
      Serial.print(F(" minutes"));
  }
  
  if (secs != 0) {
    if (mins != 0) Serial.print(F(" and "));
      Serial.print(secs);
      Serial.print(F(" seconds"));    
  }
  Serial.println();
  Serial.println();
  serialFlush();

  #if LCD_PORT
    Serial.print(F("* LCD on port "));
    Serial.print(LCD_PORT);
    Serial.println();
    serialFlush();    
  #endif

  #if DS18B_PORT

    ds18b.init();

    Serial.print(F("* DS18B on port "));
    Serial.print(DS18B_PORT);
    Serial.print(F(". Number of sensors: "));
    Serial.print(ds18b.getNumDevices());
    Serial.println();
    serialFlush();
  #endif

  #if DHT_PORT    

    dht.init();

    Serial.print(F("* DHT on port "));
    Serial.print(DHT_PORT);
    Serial.print(F(". Type: "));
    if (dht.getType() == SENSOR_NONE) {
      Serial.print(F("no device present"));
    }
    else {
      Serial.print(dht.getType());
    }
    Serial.println();
    serialFlush();
  #endif

  #if HYT131_PORT
    Serial.print(F("* HYT131 on port "));
    Serial.print(HYT131_PORT);
    Serial.println();  
    serialFlush();
  #endif

  #if LDR_PORT
    Serial.print(F("* LDR on port "));
    Serial.print(LDR_PORT);
    Serial.println();
    serialFlush();
  #endif
  
  #if PIR_PORT
    pir.digiWrite(PIR_PULLUP);

    Serial.print(F("* PIR on port "));
    Serial.print(PIR_PORT);
    Serial.println();
    serialFlush();
  #endif
  
  serialFlush();
  
  #if !DEBUG
    Serial.end();
  #endif

  // initialise transmitter
  rf12_initialize(myNodeID, RF12_868MHZ, myGroupID);

  // power down
  rf12_sleep(RF12_SLEEP);
}


/////////////////////////////////////////////////////////////////////
void loop() {

  #if PIR_PORT
    static bool pirStarted = false;

    // don't want interrupts from PIR upsetting things
    if (pirStarted) pir.disableInterrupt();
  #endif

  doMeasure();
  doReport();

  #if LCD_PORT
    displayReadingsOnLCD();
  #endif

  #if PIR_PORT
    if (pirStarted || millis() >  (1000L * PIR_STARTUP_SECS)) {
      pirStarted = true;
      pir.enableInterrupt();
    }
  #endif    

  doSleep();
}  

