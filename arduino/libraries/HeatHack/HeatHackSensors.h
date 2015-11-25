#ifndef HEATHACK_SENSORS_H
#define HEATHACK_SENSORS_H

#include <Arduino.h>
#include <avr/sleep.h>

/*
 * These includes won't get picked up properly due to the way the Arduino IDE
 * builds the include directory list for the compiler. Instead, these includes
 * need to be put in the main program file before the #include <HeatHackSensors.h> line.
*/
#include <JeeLib.h>
#include <OneWire.h>
#include "HeatHack.h"



#define DS18B_INVALID_TEMP 9999

// I2C addresses
#define MIN_I2C_ADDR 7
#define MAX_I2C_ADDR 120
#define I2C_HYT131 0x28
#define I2C_LCD 0x24


#ifndef DHT_USE_INTERRUPTS
	#define DHT_USE_INTERRUPTS true
#endif

// Counting pulse lengths depends on processor clock speed.
// Assumption here is that the tiny84 (JNMicro) is at 8MHz, otherwise 16MHz.
#if defined(__AVR_ATtiny84__)
	#define DHT_MAX_TIMER 16
	#define DHT_LONG_PULSE_CYCLES 5
#else
	#define DHT_MAX_TIMER 32
	#define DHT_LONG_PULSE_CYCLES 9
#endif

#define DHT_MAX_MICROSECS 255
#define DHT_LONG_PULSE_MICROSECS 90

#define DHT11_ACTIVATION_MS 18
#define DHT22_ACTIVATION_MS 1


// Model IDs
#define DS18S20MODEL 0x10  // also DS1820
#define DS18B20MODEL 0x28
#define DS1822MODEL  0x22
#define DS1825MODEL  0x3B

// OneWire commands
#define STARTCONVO      0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define COPYSCRATCH     0x48  // Copy EEPROM
#define READSCRATCH     0xBE  // Read EEPROM
#define WRITESCRATCH    0x4E  // Write to EEPROM
#define RECALLSCRATCH   0xB8  // Reload from last known
#define READPOWERSUPPLY 0xB4  // Determine if device needs parasite power
#define ALARMSEARCH     0xEC  // Query bus for devices with an alarm condition

// Scratchpad locations
#define TEMP_LSB        0
#define TEMP_MSB        1
#define HIGH_ALARM_TEMP 2
#define LOW_ALARM_TEMP  3
#define CONFIGURATION   4
#define INTERNAL_BYTE   5
#define COUNT_REMAIN    6
#define COUNT_PER_C     7
#define SCRATCHPAD_CRC  8

// Device resolution
#define TEMP_9_BIT  0x1F //  9 bit
#define TEMP_10_BIT 0x3F // 10 bit
#define TEMP_11_BIT 0x5F // 11 bit
#define TEMP_12_BIT 0x7F // 12 bit

// masks for getting only the valid bits of the fractional temp depending on set resolution
#define DS18B_MASK_9_BIT  0x8
#define DS18B_MASK_10_BIT 0xC
#define DS18B_MASK_11_BIT 0xE
#define DS18B_MASK_12_BIT 0xF

#define DS18B_HALF      0x8
#define DS18B_QUARTER   0x4
#define DS18B_EIGHTH    0x2
#define DS18B_SIXTEENTH 0x1

// time in milliseconds to wait after turning on sensor power
#define DS18B_POWERUP_TIME_MS 50

// resolution (precision)
#define DS18B_RESOLUTION TEMP_12_BIT
#define DS18B_TEMP_MASK DS18B_MASK_12_BIT


// time in milliseconds to wait for DS18 sensors to take a reading
// depends on precision:
//   9 bits:  94ms
//  10 bits: 188ms
//  11 bits: 375ms
//  12 bits: 750ms
#define DS18B_READ_TIME_MS 750


class Sensor : public Port {

public:
	Sensor(byte portNum)
		: Port(portNum) {
	}
	
	virtual void init(void) = 0;
	virtual void reading(HeatHackData& packet) = 0;
};

extern void serialFlush (void);


/**********************************************************************************
 * Interface for the DHT11 and DHT22 sensors.
 * Does not use floating point, therefore results are returned in tenths of a unit,
 * e.g. 25.4 is returned as 254.
 *
 * Based on Jeelib's DHTxx class for the poll-based read and conversion of raw data to results,
 * and idDHT11 for the interrupt-based read.
 *
 * Sensor's data line should be connected to the D  pin. Reading the data can be interrupt-driven
 * or polled. Interrupt is preferred as it will be lower powered, but could cause problems if other
 * interrupt-driven devices are also in use (eg PIR motion sensor). Also, the interrupt version
 * of the code is slightly larger.
 * 
 * If sensor's power is connected to A instead of +, it will only be turned on
 * when taking a reading to save power.
 *
 * So, ideal setup for connecting the sensor to the JeeNode port is:
 *  sensor  port
 *  GND/-   G
 *  VCC/+   A
 *  DATA    D
 * 
 * The interrupt version is the default.
 * To use polling instead, add the line: #define DHT_USE_INTERRUPTS false
 *
 * Sleepy is used for delays, therefore ISR(WDT_vect) { Sleepy::watchdogEvent(); }
 * must be included in the main sketch.
 */
class DHT : public Sensor {
  byte type;
  uint8_t dataPin;

  // Note that variables used for interrupt-based capture are declared static
  // so that the interrupt handler can access them.
  
#if DHT_USE_INTERRUPTS
  volatile static uint8_t data[5]; // holds the raw data
  volatile static uint8_t currentBit;

  // used specifically during interrupt-based capture
  volatile static uint32_t pulseStartTime;
  volatile static bool acquiring;
  volatile static bool acquiredSuccessfully;
  volatile static uint8_t intDataPin;
#else
  static uint8_t data[5]; // holds the raw data
  static uint8_t currentBit;
#endif
  
public:
  /**
   * portNum: 1, 2, 3 or 4
   * sensorType: DHT11_TYPE or DHT22_TYPE
   */
  DHT (byte portNum, byte type)
  : Sensor(portNum), type(type), dataPin(0) {
  }

  DHT (byte portNum)
  : Sensor(portNum), type(SENSOR_NONE), dataPin(0) {
  }

  // test for the presence of a DHT sensor on the given port
  static byte testPort(byte portNum, bool useIPin = false) {
    if (testSensor(portNum, useIPin, DHT22_ACTIVATION_MS)) return SENSOR_DHT22;
    else if (testSensor(portNum, useIPin, DHT11_ACTIVATION_MS)) return SENSOR_DHT11;
    else return SENSOR_NONE;
  }

  void init(void) {
    _init(false);
  }

  void initOnIPin(void) {
    _init(true);
  }

  inline uint8_t getType(void) {
	return type;
  }
  
  void reading (HeatHackData& packet) {

    // nasty hack, but without it the DHT only works if DEBUG
    // is enabled or the LCD is plugged in!
    delay(10);

    if (type == SENSOR_NONE) return;

    enablePower();
    bool success = readRawData();

#if DEBUG
    if (!success) {
      Serial.println(F("DHT readRawData failed"));
      serialFlush();
    }
    else {
        Serial.print(F("DHT data: "));
        Serial.print(data[0], DEC);
        Serial.print(F(" "));
        Serial.print(data[1], DEC);
        Serial.print(F(" "));
        Serial.print(data[2], DEC);
        Serial.print(F(" "));
        Serial.print(data[3], DEC);
        Serial.print(F(" "));
        Serial.println(data[4], DEC);
        serialFlush();
    }
#endif

    disablePower();

    if (success) {
      int temp = 0, humi = 0;

      if (computeResults(temp, humi)) {
        HHReading reading;
        reading.setPort(portNum);
        reading.setSensor(1);
        reading.sensorType = HHSensorType::TEMPERATURE;
        reading.encodedReading = temp;
        packet.addReading(reading);

        reading.setSensor(2);
        reading.sensorType = HHSensorType::HUMIDITY;
        reading.encodedReading = humi;
        packet.addReading(reading);
      }
    }

    return;
  }

#if DHT_USE_INTERRUPTS  
  // the interrupt handler
  static void isrCallback() {
    // only interested in the falling edge
    if (digitalRead(intDataPin) != LOW) return;

    uint32_t pulseEndTime = micros();
    unsigned int delta = (pulseEndTime - pulseStartTime);
    pulseStartTime = pulseEndTime;

    if (delta > DHT_MAX_MICROSECS) {
      acquiring = false;
      return;
    }

    // ignore first 'bit' as it's really a sync pulse
    if (currentBit >= 1 ) {

      // collect each bit in the data buffer
      byte offset = (currentBit-1) >> 3; // divide by 8
      data[offset] <<= 1;  // shift existing bits left to make way for next one

      // short pulse is a 0, long pulse a 1
      data[offset] |= (delta >= DHT_LONG_PULSE_MICROSECS);
    }
  
    currentBit++;

    if (currentBit == 41) {
      acquiring = false;
      acquiredSuccessfully = true;
    }
  }
#endif
  
protected:
  
  void _init(bool useIPin) {
    #if defined(__AVR_ATtiny84__)
      // select between I pin, D on port 1 or D on port 2
      dataPin = useIPin ? 3 : 12 - 2 * portNum;
    #else
      // I pin for DHT data only supported on micro. Standard Jeenode always uses D pin
      dataPin = portNum + 3;
    #endif

    if (type == SENSOR_NONE) {
      enablePower();
      type = testPort(portNum, useIPin);
      disablePower();
    }
  }

  inline void enablePower(void) {
    // turn on A pin to power up sensor
    mode2(OUTPUT);
    digiWrite2(HIGH);

    // Set data pin as an output initially for signalling sensor to take a reading.
    // Start with output high.

    // let pull-up resistor pull data bus high
    pinMode(dataPin, INPUT);
    digitalWrite(dataPin, LOW);

    // sensor needs 1 sec to stabilise after power is applied
    Sleepy::loseSomeTime(1000);
  }
  
  inline void disablePower(void) {
    // turn off pullup resistor to save power
    #if defined(__AVR_ATtiny84__)
    digitalWrite(dataPin, LOW);
    #else
    pinMode(dataPin, INPUT);
    #endif

    // turn off A pin
    digiWrite2(LOW);
  }
  
  // return true if sensor responds within given activationTime
  static bool testSensor(byte portNum, bool useIPin, uint8_t activationTime) {
  
    byte pin;

    #if defined(__AVR_ATtiny84__)
      // select between I pin, D on port 1 or D on port 2
      pin = useIPin ? 3 : 12 - 2 * portNum;
    #else
      // I pin for DHT data only supported on micro. Standard Jeenode always uses D pin
      pin = portNum + 3;
    #endif

    // pull data line low to activate sensor
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    // wait for min period required for the sensor
    delay(activationTime);

    // set bus back to high to indicate we're ready to receive the result
    digitalWrite(pin, HIGH);

    // sensor starts its response by pulling bus low within 40us
    delayMicroseconds(40);

    // enable pullup resistor for the input in case no sensor's connected
    #if defined(__AVR_ATtiny84__)
      // INPUT_PULLUP not supported yet in the libs
      pinMode(pin, INPUT);
      digitalWrite(pin, HIGH);
    #else
      pinMode(pin, INPUT_PULLUP);
    #endif
  
    // sensor should have pulled line low by now
    if (digitalRead(pin) == LOW) return true;
    else return false;
  }
  
  inline void initiateReading(void) {
    // pull bus low for >18ms to send start signal to sensor
    // Sleepy uses watchdog timer, which has 16ms resolution
    pinMode(dataPin, OUTPUT);
    digitalWrite(dataPin, LOW);

    if (type == SENSOR_DHT22) {
      delay(DHT22_ACTIVATION_MS);
    }
    else {
      Sleepy::loseSomeTime(DHT11_ACTIVATION_MS);
    }

    // enable pullup resistor for the input in case no sensor's connected -
    // it will be easier to detect an unchanging input if it's pulled up
    // rather than floating.

    // release bus to indicate we're ready for sensor's response
    #if defined(__AVR_ATtiny84__)
      // INPUT_PULLUP not supported yet in the libs
      pinMode(dataPin, INPUT);
      digitalWrite(dataPin, HIGH);
    #else
      pinMode(dataPin, INPUT_PULLUP);
    #endif

    // sensor starts its response by pulling bus low within 40us
    // want to wait for it to have gone low before starting the reading
    delayMicroseconds(40);
/*
    // sensor should have pulled line low by now
    if (digitalRead(dataPin) == HIGH) {
      // looks like sensor's not responding
      sei();
      return false;
    }

    return true;
*/
/*
    uint8_t bits[100];
    byte offset;

    cli();

    for (int i=0; i<800; i++) {

      // collect each bit in the data buffer
      offset = i >> 3;
      bits[offset] <<= 1;  // shift existing bits left to make way for next one

      if (digitalRead(dataPin) == HIGH) bits[offset] |= 1;
    }

    sei();

    for (int i=0; i<100; i++) {
      Serial.print(bits[i], BIN);
    }

    Serial.println();
    serialFlush();
*/
  }


#if DHT_USE_INTERRUPTS  
  inline bool readRawData(void) {

    initiateReading();

    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();

    acquiring = true;
    acquiredSuccessfully = false;
    currentBit = 0;
    intDataPin = dataPin;
    pulseStartTime = micros();

    // enable interrupt
    #if defined(__AVR_ATtiny84__)
      bitSet(PCMSK0, portNum * 2 - 2); // port1 = PCINT0, port2 = PCINT2
      bitSet(GIMSK, PCIE0);
    #else
      bitSet(PCMSK2, dataPin);
      bitSet(PCICR, PCIE2);
    #endif

    do {
      // wait for interrupt
      sleep_cpu();
    } while (acquiring);

    sleep_disable();

    // disable interrupt
    #if defined(__AVR_ATtiny84__)
      bitClear(GIMSK, PCIE0);
      bitClear(PCMSK0, portNum * 2 - 2); // port1 = PCINT0, port2 = PCINT2
    #else
      bitClear(PCICR, PCIE2);
      bitClear(PCMSK2, dataPin);
    #endif

    return acquiredSuccessfully;
  }
  
#else  

  inline bool readRawData(void) {
  
    initiateReading();

    // clear interrupts to avoid timing problems
    cli();

    // each bit is a high edge followed by a variable length low edge
    for (byte currentBit = 0; currentBit < 41; currentBit++) {

      // wait for the high edge, then measure time until the low edge
      byte timer;

      // can't use micros() here for measuring the length of the pulse as interrupts have been
      // disabled, so instead simply count the number of times round the timer loop
      for (byte inputLevel = 0; inputLevel < 2 && timer < DHT_MAX_TIMER; inputLevel++) {
        for (timer = 0; timer < DHT_MAX_TIMER; timer++) {
          if (digitalRead(dataPin) != inputLevel) {
            break;
          }
        }
      }

      // if no transition was seen, return
      if (timer >= DHT_MAX_TIMER) {
        sei();
        return false;
      }
      else {
        // ignore first 'bit' as it's really a sync pulse
        if (currentBit >= 1 ) {

          // collect each bit in the data buffer
          byte offset = (currentBit-1) >> 3; // divide by 8
          data[offset] <<= 1;  // shift existing bits left to make way for next one

          // short pulse is a 0, long pulse a 1
          data[offset] |= (timer >= DHT_LONG_PULSE_CYCLES);
        }
      }
    }

    // re-enable interrupts
    sei();
    return true;
  }

#endif
  
  inline bool computeResults(int& temp, int& humi) {
    // calculate checksum to ensure data is valid
    byte sum = data[0] + data[1] + data[2] + data[3];
    if (sum != data[4]) {
      return false;
    }

    int t;

    // higher precision DHT22 uses 2 bytes instead of 1
    if (type == SENSOR_DHT22) {
      humi =  (data[0] << 8) | data[1];
      t = ((data[2] & 0x7F) << 8) | data[3];
    }
    else {
      humi =  10 * data[0];
      t = 10 * data[2];
    }

    // is temperature negative?
    temp = data[2] & 0x80 ? - t : t;

    return true;
  }
};

#if DHT_USE_INTERRUPTS
volatile uint8_t DHT::data[5]; // holds the raw data
volatile uint8_t DHT::currentBit;

volatile uint32_t DHT::pulseStartTime;
volatile bool DHT::acquiring;
volatile bool DHT::acquiredSuccessfully;
volatile uint8_t DHT::intDataPin;
#else
uint8_t DHT::data[5]; // holds the raw data
uint8_t DHT::currentBit;
#endif

#if DHT_USE_INTERRUPTS  
	#if defined(__AVR_ATtiny84__)
		#define DHT_INT PCINT0_vect
	#else
		#define DHT_INT PCINT2_vect
	#endif

	ISR(DHT_INT) { DHT::isrCallback(); }
#endif


typedef uint8_t DeviceAddress[8];
typedef uint8_t ScratchPad[9];

/**********************************************************************************
 * Interface for the Dallas DS18B20 sensors.
 * Note there is no support for the DS18Sxx and DS18xx sensors to simplfy the code.
 */
class DS18B : public Sensor {

  uint8_t numDevices;
  DeviceAddress deviceAddress[3];
  OneWire oneWire;
  
public:
  
  DS18B (byte portNum)
	: Sensor(portNum), numDevices(0) {

	// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
	oneWire.init(digiPin());
  }
  
  // initialise the bus and determine the available sensors
  // note this can only be called once as search isn't reset
  void init(void) {
	enablePower();
  
	// get addresses of first 3 devices	
    while (numDevices < 3 && oneWire.search(deviceAddress[numDevices])) {
		// check address is valid
        if (oneWire.crc8(deviceAddress[numDevices], 7) == deviceAddress[numDevices][7]) {
            numDevices++;
        }
    }

	// set resolution
	for (uint8_t i=0; i < numDevices; i++) {
		setResolution(deviceAddress[i], DS18B_RESOLUTION);
	}
	
	disablePower();
  }
  
  // take readings for all connected devices
  void reading (HeatHackData& packet) {

    enablePower();
      requestTemperatures();

    HHReading reading;
    reading.setPort(portNum);
    reading.sensorType = HHSensorType::TEMPERATURE;

    for (uint8_t i=0; i < numDevices; i++) {
      reading.setSensor(i + 1);
      reading.encodedReading = getTemp(deviceAddress[i]);

      if (reading.encodedReading != DS18B_INVALID_TEMP) {
        packet.addReading(reading);
      }
    }

    disablePower();
  }
  
  uint8_t getNumDevices(void) {
	return numDevices;
  }
  
protected:

  void enablePower(void) {
	// turn on A pin
	mode2(OUTPUT);
    digiWrite2(HIGH);
	
	// set data pin as input
	mode(INPUT);
	
    Sleepy::loseSomeTime(DS18B_POWERUP_TIME_MS);
  }

  void disablePower(void) {
    // turn off A pin
	digiWrite2(LOW);
	
	// ensure data pin doesn't have a pull-up configured
	digiWrite(LOW);
  }
  

  // set resolution of a device to 9, 10, 11, or 12 bits
  // if new resolution is out of range, 9 bits is used.
  inline bool setResolution(const uint8_t* deviceAddress, uint8_t newResolution) {
	ScratchPad scratchPad;
	
	// note isConnected() also reads the scratchpad
	if (isConnected(deviceAddress, scratchPad))
	{
		scratchPad[CONFIGURATION] = newResolution;
		writeScratchPad(deviceAddress, scratchPad);
		return true;  // new value set
	}
	return false;
  }

  // sends command for all devices on the bus to perform a temperature conversion
  inline void requestTemperatures() {
    oneWire.reset();
    oneWire.skip();
    oneWire.write(STARTCONVO, true);

    Sleepy::loseSomeTime(DS18B_READ_TIME_MS);
  }

  // returns temperature in 1/10 degrees C or DS18_INVALID_TEMP if the
  // device's scratch pad cannot be read successfully.
  inline int16_t getTemp(const uint8_t* deviceAddress) {
    ScratchPad scratchPad;

    if (!isConnected(deviceAddress, scratchPad)) {
      return DS18B_INVALID_TEMP;
    }
    else {
      int16_t temp =
        // start with the whole degrees
        ((((int16_t) scratchPad[TEMP_MSB]) << 4) | (scratchPad[TEMP_LSB] >> 4)) * 10;

      // add on the 1/10ths

      // depending on the resolution, some of the low bits are undefined, so mask them off
      uint8_t fraction16ths = scratchPad[TEMP_LSB] & DS18B_TEMP_MASK;

      // convert fraction in 1/16ths to 1/10ths
      if (fraction16ths & DS18B_HALF)      temp += 5;
      if (fraction16ths & DS18B_QUARTER)   temp += 2;
      if (fraction16ths & DS18B_EIGHTH)    temp += 1;
      if (fraction16ths & DS18B_SIXTEENTH) temp += 1;

      return temp;
    }
  }
  

  // attempt to determine if the device at the given address is connected to the bus
  // also allows for updating the read scratchpad
  bool isConnected(const uint8_t* deviceAddress, uint8_t* scratchPad) {
    readScratchPad(deviceAddress, scratchPad);

    // if device gets disconnected, scratchpad will be all zeros, but the
    // crc will be correct :(
    if (scratchPad[0] == 0 &&
        scratchPad[1] == 0 &&
        scratchPad[2] == 0 &&
        scratchPad[3] == 0 &&
        scratchPad[4] == 0 &&
        scratchPad[5] == 0 &&
        scratchPad[6] == 0 &&
        scratchPad[7] == 0 &&
        scratchPad[8] == 0) {
      return false;
    }
    else {
      return (oneWire.crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]);
    }
  }

  // read device's scratch pad
  void readScratchPad(const uint8_t* deviceAddress, uint8_t* scratchPad) {
    // send the command
    oneWire.reset();
    oneWire.select(deviceAddress);
    oneWire.write(READSCRATCH);

    // read the response

    for (uint8_t i=0; i<9; i++) {
      *scratchPad = oneWire.read();
      scratchPad++;
    }
	
    // byte 0: temperature LSB
    // byte 1: temperature MSB
    // byte 2: high alarm temp
    // byte 3: low alarm temp
    // byte 4:
    // DS18S20: store for crc
    // DS18B20 & DS1822: configuration register
    // byte 5:
    // internal use & crc
    // byte 6:
    // DS18S20: COUNT_REMAIN
    // DS18B20 & DS1822: store for crc
    // byte 7:
    // DS18S20: COUNT_PER_C
    // DS18B20 & DS1822: store for crc
    // byte 8:
    // SCTRACHPAD_CRC

    oneWire.reset();

#if DEBUG
    Serial.print(F("DS18B data: "));
    scratchPad -= 9;
    for (uint8_t i=0; i<9; i++) {
      Serial.print(scratchPad[i], DEC);
      Serial.print(F(" "));
    }
    Serial.println();
    serialFlush();
#endif
  }

  // writes device's scratch pad
  void writeScratchPad(const uint8_t* deviceAddress, const uint8_t* scratchPad) {
    oneWire.reset();
    oneWire.select(deviceAddress);
    oneWire.write(WRITESCRATCH);
    oneWire.write(scratchPad[HIGH_ALARM_TEMP]); // high alarm temp
    oneWire.write(scratchPad[LOW_ALARM_TEMP]); // low alarm temp
    // DS1820 and DS18S20 have no configuration register
    if (deviceAddress[0] != DS18S20MODEL) oneWire.write(scratchPad[CONFIGURATION]); // configuration
    oneWire.reset();
    oneWire.select(deviceAddress); //<--this line was missing
    // save the newly written values to eeprom
    oneWire.write(COPYSCRATCH, true);
	// 10ms delay for parasitic power
    Sleepy::loseSomeTime(16);
    oneWire.reset();
  }
  
};

#define BUS_UNKNOWN 0
#define BUS_SWITCHED_POWER 1
#define BUS_I2C 2

class Autodetect : public PortI2C {

public:
	Autodetect() : PortI2C(1) {
	}
	
	void probePorts(byte typeList[4]) {
		for (byte port = 0; port <4; port++) {
			typeList[port] = probePort(port + 1);
		}
	}
	
	byte probePort(byte portNum) {
		this->portNum = portNum;
		
		byte type = SENSOR_NONE;
		
		// determine what kind of bus we have
		byte bus = determineBus();

		if (bus == BUS_I2C) {
      #if DEBUG
        Serial.print(F(" (I2C) "));
        Serial.flush();
      #endif

			type = probeI2C();

			if (type == SENSOR_NONE) {
				// maybe not an i2c bus after all
				type = probeSwitchedPower();
			}
		}
		else if (bus == BUS_SWITCHED_POWER) {
      #if DEBUG
			  Serial.print(F(" (SwPwr) "));
			  Serial.flush();
      #endif

			type = probeSwitchedPower();
		}
#if DEBUG
		else {
			Serial.print(F(" (Unknown) "));
		}
#endif
		
		// turn off A pin power
		mode2(INPUT);
		digiWrite2(LOW);
		
		// ensure data pin doesn't have a pull-up configured
		mode(INPUT);
		digiWrite(LOW);		
		
		return type;
	}
	
private:
	byte probeI2C(void) {
		byte type = SENSOR_NONE;

		// set up I2C bus
		sdaOut(1);
		mode2(OUTPUT);
		sclHi();
		delay(1);

		// check if this is really an I2C bus. SDA should be high at this point
		if (sdaIn()) {

			// check all addresses
			// if more than one responds, assume it's not an I2C bus as we're assuming one device per port
			uint8_t activeAddr = 255;

			for (byte a=0; a<128; a++ ) {
				if (checkI2C(a)) {
					if (activeAddr == 255) {
						activeAddr = a;
					}
					else {
						// got a second response - stop checking
						activeAddr = 255;
						break;
					}
				}
			}

			if (activeAddr >= MIN_I2C_ADDR && activeAddr <= MAX_I2C_ADDR) {
				switch (activeAddr) {
				case I2C_HYT131: type = SENSOR_HYT131; break;
				case I2C_LCD: type = SENSOR_LCD; break;
				default: type = SENSOR_I2C_UNKNOWN;
				}
			}
		}

		return type;
	}

	byte probeSwitchedPower(void) {
		byte type = SENSOR_NONE;

		// set up bus
		mode(INPUT);
		digiWrite(LOW);

		// turn on A pin power
		mode2(OUTPUT);
		digiWrite2(HIGH);
		Sleepy::loseSomeTime(DS18B_POWERUP_TIME_MS);

		// 1. check for a OneWire bus (DS18B20 sensors)
		OneWire oneWire;
		oneWire.init(digiPin());
		DeviceAddress ds18Addr;

		if (oneWire.search(ds18Addr) &&
			oneWire.crc8(ds18Addr, 7) == ds18Addr[7]) {

			type = SENSOR_DS18B;
		}

		if (type == SENSOR_NONE) {
			// 2. then for DHT11/22

			// Set data pin as an output initially for signalling sensor to take a reading.
			// Start with output high.
			mode(OUTPUT);
			digiWrite(HIGH);
			Sleepy::loseSomeTime(1000);

			type = DHT::testPort(portNum);
		}

		return type;
	}

	byte determineBus(void) {
		// set DIO (SDA) to a floating input
		mode(INPUT);
		digiWrite(LOW);
		
		// set AIO (power/SCL) low
		mode2(OUTPUT);
		digiWrite2(LOW);
		
		// check what's on DIO
		delay(DS18B_POWERUP_TIME_MS);
		byte DFWhenALow = digiRead();

		// pull up DIO
		digiWrite(HIGH);
		
		// check what's on DIO
		delay(DS18B_POWERUP_TIME_MS);
		byte DPWhenALow = digiRead();
		
		// set AIO (power/SCL) high
		digiWrite2(HIGH);
		
		// check what's on DIO
		delay(DS18B_POWERUP_TIME_MS);
		byte DPWhenAHigh = digiRead();

		// and DIO back to floating
		digiWrite(LOW);

		// check what's on DIO
		delay(DS18B_POWERUP_TIME_MS);
		byte DFWhenAHigh = digiRead();

#if DEBUG
		Serial.print(DFWhenAHigh ? "H" : "L");
		Serial.print(DFWhenALow ? "H" : "L");
		Serial.print(DPWhenAHigh ? "H" : "L");
		Serial.print(DPWhenALow ? "H" : "L");
#endif
		
		// what does it all mean?
		if (DFWhenAHigh && !DFWhenALow && DPWhenAHigh && !DPWhenALow) {
			// looks like A is connected to D (via pullup resistor)
			return BUS_SWITCHED_POWER;
		}
		else if (DPWhenAHigh && DPWhenALow) {
			// A doesn't seem to affect D
			return BUS_I2C;
		}
		else {
			// D pulled low when not expected
			return BUS_UNKNOWN;
		}
		
	}

	// takes a 7 bit address
	bool checkI2C(byte addr) {
	
		// need to pass address as top 7 bits with lsb indicating read/write
		bool result = start(addr << 1);
		delayMicroseconds(150); // from HYT131 datasheet
		stop();
		return result;
	}
};

#endif
