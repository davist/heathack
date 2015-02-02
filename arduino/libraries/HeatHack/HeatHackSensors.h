#ifndef HEATHACK_SENSORS_H
#define HEATHACK_SENSORS_H

#include <avr/sleep.h>

/*
 * These includes won't get picked up properly due to the way the Arduino IDE
 * builds the include directory list for the compiler. Instead, these includes
 * need to be put in the main program file before the #include <HeatHackSensors.h> line.
 
#include <JeeLib.h>

#define ONEWIRE_CRC8_TABLE 0
#include <OneWire.h>
*/

#define DS18B_INVALID_TEMP 9999


#ifndef DHT_USE_INTERRUPTS
	#define DHT_USE_INTERRUPTS true
#endif

#ifndef DHT_USE_I_PIN_FOR_DATA
	#define DHT_USE_I_PIN_FOR_DATA false
#endif

#if DHT_USE_I_PIN_FOR_DATA
	#define DHT_DATA_READ digiRead3
	#define DHT_DATA_WRITE digiWrite3
	#define DHT_DATA_MODE mode3
	#define DHT_DATA_PIN digiPin3
#else
	#define DHT_DATA_READ digiRead
	#define DHT_DATA_WRITE digiWrite
	#define DHT_DATA_MODE mode
	#define DHT_DATA_PIN digiPin
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

#define DHT_TYPE_NONE 0
#define DHT11_TYPE 11
#define DHT22_TYPE 22

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
class DHT : public Port {
  byte type;
  
  // Note that variables used for interrupt-based capture are declared static
  // so that the interrupt handler can access them.
  
  volatile static uint8_t data[5]; // holds the raw data
  volatile static uint8_t currentBit;

#if DHT_USE_INTERRUPTS  
  // used specifically during interrupt-based capture
  volatile static uint32_t pulseStartTime;
  volatile static bool acquiring;
  volatile static bool acquiredSuccessfully;
  volatile static uint8_t dataPin;
#endif
  
public:
  /**
   * portNum: 1, 2, 3 or 4
   * sensorType: DHT11_TYPE or DHT22_TYPE
   */
  DHT (byte portNum)
	: Port(portNum) {
	type = DHT_TYPE_NONE;
  }
  
  // initialise object by detecting the type of sensor connected (if there is one)
  uint8_t init() {
	enablePower();

	if (testSensor(DHT22_ACTIVATION_MS)) {
		type = DHT22_TYPE;
	}
	else if (testSensor(DHT11_ACTIVATION_MS)) {
		type = DHT11_TYPE;
	}
	// else no sensor present. Leave type at default of none.

	disablePower();
	
	return type;
  }

  bool reading (int& temp, int& humi) {

	// don't try reading if no sensor present or not initialised
	if (type == DHT_TYPE_NONE) return false;
  
	enablePower();	

	bool success;

	success = initiateReading();

	#if DEBUG
	if (!success) Serial.println("initiateReading failed");
	#endif
	
	if (success) {
		success = readRawData();

		#if DEBUG
		if (!success) Serial.println("readRawData failed");
		#endif
	}
	
	disablePower();

	#if DEBUG
	Serial.print("data: ");
	Serial.print(data[0], DEC);
	Serial.print(" ");
	Serial.print(data[1], DEC);
	Serial.print(" ");
	Serial.print(data[2], DEC);
	Serial.print(" ");
	Serial.print(data[3], DEC);
	Serial.print(" ");
	Serial.println(data[4], DEC);
	#endif

	if (success) {
		success = computeResults(temp, humi);
	}
	return success;
  }

#if DHT_USE_INTERRUPTS  
  // the interrupt handler
  static void isrCallback() {
	// only interested in the falling edge
	if (digitalRead(dataPin) != LOW) return;
  
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
  
  inline void enablePower(void) {
	// turn on A pin to power up sensor
	mode2(OUTPUT);
	digiWrite2(HIGH);
	
	// Set data pin as an output initially for signalling sensor to take a reading.
	// Start with output high.
	DHT_DATA_MODE(OUTPUT);
	DHT_DATA_WRITE(HIGH);
	
	// sensor needs 1 sec to stabilise after power is applied
	Sleepy::loseSomeTime(1000);
  }
  
  inline void disablePower(void) {
	// turn off pullup resistor to save power
	#if defined(__AVR_ATtiny84__)
		DHT_DATA_WRITE(LOW);
	#else
		DHT_DATA_MODE(INPUT);
	#endif
	
	// turn off A pin
	digiWrite2(LOW);
  }
  
  // return true if sensor responds within given activationTime
  bool testSensor(uint8_t activationTime) {
	// pull data line high to activate sensor
	DHT_DATA_MODE(OUTPUT);
	DHT_DATA_WRITE(LOW);
	
	// wait for min period required for the sensor
	delay(activationTime);

	// set bus back to high to indicate we're ready to receive the result
	DHT_DATA_WRITE(HIGH);

	// sensor starts its response by pulling bus low within 40us
	delayMicroseconds(40);
	
	// enable pullup resistor for the input in case no sensor's connected
	#if defined(__AVR_ATtiny84__)
		// INPUT_PULLUP not supported yet in the libs
		DHT_DATA_MODE(INPUT);
		DHT_DATA_WRITE(HIGH);
	#else
		DHT_DATA_MODE(INPUT_PULLUP);
	#endif

	// sensor should have pulled line low by now
	if (DHT_DATA_READ() == LOW) return true;
	else return false;
  }
  
  inline bool initiateReading(void) {
	// pull bus low for >18ms to send start signal to sensor
	// Sleepy uses watchdog timer, which has 16ms resolution
	DHT_DATA_WRITE(LOW);
	Sleepy::loseSomeTime(32);

	// set bus back to high to indicate we're ready to receive the result
	DHT_DATA_WRITE(HIGH);

	// sensor starts its response by pulling bus low within 40us
	// want to wait for it to have gone low before starting the reading
	delayMicroseconds(40);
	
	// enable pullup resistor for the input in case no sensor's connected -
	// it will be easier to detect an unchanging input if it's pulled up
	// rather than floating.
	#if defined(__AVR_ATtiny84__)
		// INPUT_PULLUP not supported yet in the libs
		DHT_DATA_MODE(INPUT);
		DHT_DATA_WRITE(HIGH);
	#else
		DHT_DATA_MODE(INPUT_PULLUP);
	#endif

	// sensor should have pulled line low by now
	if (DHT_DATA_READ() != LOW) {
		// looks like sensor's not responding
		return false;
	}

	return true;
  }

#if DHT_USE_INTERRUPTS  
  inline bool readRawData(void) {
	bool result = true;

	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

	acquiring = true;
	acquiredSuccessfully = false;
	currentBit = 0;
	dataPin = DHT_DATA_PIN();
	pulseStartTime = micros();

	// enable interrupt
	#if defined(__AVR_ATtiny84__)
		#if DHT_USE_I_PIN_FOR_DATA
			bitSet(PCMSK0, PCINT7);
		#else
			bitSet(PCMSK0, portNum * 2 - 2); // port1 = PCINT0, port2 = PCINT2
		#endif		
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
		#if DHT_USE_I_PIN_FOR_DATA
			bitClear(PCMSK0, PCINT7);
		#else
			bitClear(PCMSK0, portNum * 2 - 2); // port1 = PCINT0, port2 = PCINT2
		#endif		
	#else
		bitClear(PCICR, PCIE2);
		bitClear(PCMSK2, dataPin);
	#endif
	
	return acquiredSuccessfully;
  }
  
#else  

  inline bool readRawData(void) {
  
	bool result = true;
	
	// disable interrupts while receiving data
	cli();

	// each bit is a high edge followed by a variable length low edge
	for (byte currentBit = 0; currentBit < 41; currentBit++) {

		// wait for the high edge, then measure time until the low edge
		byte timer;
		
		// can't use micros() here for measuring the length of the pulse as interrupts have been
		// disabled, so instead simply count the number of times round the timer loop
		for (byte inputLevel = 0; inputLevel < 2; inputLevel++) {
			for (timer = 0; timer < DHT_MAX_TIMER; timer++) {
				if (DHT_DATA_READ() != inputLevel) {
					break;
				}
			}
		}

		// if no transition was seen, return 
		if (timer == DHT_MAX_TIMER) {
			result = false;
			break;
		}
		
		// ignore first 'bit' as it's really a sync pulse
		if (currentBit >= 1 ) {

			// collect each bit in the data buffer
			byte offset = (currentBit-1) >> 3; // divide by 8
			data[offset] <<= 1;  // shift existing bits left to make way for next one
			
			// short pulse is a 0, long pulse a 1
			data[offset] |= (timer >= DHT_LONG_PULSE_CYCLES);
		}
	}

	// re-enable interrupts
	sei();

	return result;
  }

#endif
  
  inline bool computeResults(int& temp, int& humi) {
	// calculate checksum to ensure data is valid
	byte sum = data[0] + data[1] + data[2] + data[3];
	if (sum != data[4]) {
		return false;
	}

	word t;

	// higher precision DHT22 uses 2 bytes instead of 1
	if (type == DHT22_TYPE) {
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

volatile uint8_t DHT::data[5]; // holds the raw data
volatile uint8_t DHT::currentBit;

#if DHT_USE_INTERRUPTS
volatile uint32_t DHT::pulseStartTime;
volatile bool DHT::acquiring;
volatile bool DHT::acquiredSuccessfully;
volatile uint8_t DHT::dataPin;
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
class DS18B : public Port {

  uint8_t numDevices;
  DeviceAddress deviceAddress[3];
  OneWire oneWire;
  
public:
  
  DS18B (byte portNum)
	: Port(portNum) {

	// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
	oneWire.init(digiPin());
  }
  
  // initialise the bus and determine the available sensors
  // note this can only be called once as search isn't reset
  uint8_t init(void) {
	enablePower();
  
	// get addresses of first 3 devices	
    numDevices = 0;

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
	
	return numDevices;
  }
  
  // take readings for all connected devices
  // results returned in the passed in temp array
  void reading(int16_t* temp) {

	enablePower();	

    requestTemperatures();

	for (uint8_t i=0; i < numDevices; i++) {
		temp[i] = getTemp(deviceAddress[i]);
    }
	
	disablePower();
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
    return (oneWire.crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]);
  }

  // read device's scratch pad
  void readScratchPad(const uint8_t* deviceAddress, uint8_t* scratchPad) {
    // send the command
    oneWire.reset();
    oneWire.select(deviceAddress);
    oneWire.write(READSCRATCH);

    // read the response

    // byte 0: temperature LSB
    scratchPad[TEMP_LSB] = oneWire.read();

    // byte 1: temperature MSB
    scratchPad[TEMP_MSB] = oneWire.read();

    // byte 2: high alarm temp
    scratchPad[HIGH_ALARM_TEMP] = oneWire.read();

    // byte 3: low alarm temp
    scratchPad[LOW_ALARM_TEMP] = oneWire.read();

    // byte 4:
    // DS18S20: store for crc
    // DS18B20 & DS1822: configuration register
    scratchPad[CONFIGURATION] = oneWire.read();

    // byte 5:
    // internal use & crc
    scratchPad[INTERNAL_BYTE] = oneWire.read();

    // byte 6:
    // DS18S20: COUNT_REMAIN
    // DS18B20 & DS1822: store for crc
    scratchPad[COUNT_REMAIN] = oneWire.read();

    // byte 7:
    // DS18S20: COUNT_PER_C
    // DS18B20 & DS1822: store for crc
    scratchPad[COUNT_PER_C] = oneWire.read();

    // byte 8:
    // SCTRACHPAD_CRC
    scratchPad[SCRATCHPAD_CRC] = oneWire.read();

    oneWire.reset();
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

#endif
