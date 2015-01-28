#ifndef HEATHACK_SENSORS_H
#define HEATHACK_SENSORS_H

#include <jeelib.h>
#include <avr/sleep.h>

// time in microseconds to wait after turning on sensor power
#define DS18_POWERUP_TIME 50 

#define DHT11_TYPE 11
#define DHT22_TYPE 22

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
 * interrupt-driven devices are also in use (eg PIR motion sensor).
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
 * If using interrupts the line: ISR(PCINT2_vect) { DHT::isrCallback(); }
 * must be included in the main sketch.
 * Sleepy is used for delays, therefore ISR(WDT_vect) { Sleepy::watchdogEvent(); }
 * must also be included in the main sketch.
 */
class DHT : public Port {
  byte type;
  bool useInterrupts;
  
  // Note that variables used for interrupt-based capture are declared static
  // so that the interrupt handler can access them.
  
  volatile static uint8_t data[5]; // holds the raw data
  volatile static uint8_t currentBit;
	
  // used specifically during interrupt-based capture
  volatile static uint32_t pulseStartTime;
  volatile static bool acquiring;
  volatile static bool acquiredSuccessfully;
  volatile static uint8_t dataPin;
  
public:
  /**
   * portNum: 1, 2, 3 or 4
   * sensorType: DHT11_TYPE or DHT22_TYPE
   * useInterrupts: true - reading data done via interrupts to save power.
   *   false - a busy loop used to read the signal.
   */
  DHT (byte portNum, byte sensorType, bool useInterrupts)
	: Port(portNum), type(sensorType), useInterrupts(useInterrupts) {
  }
  
  // Results are returned in tenths of a degree and percent, respectively.
  // i.e 25.4 is returned as 254.
  bool reading (int& temp, int& humi) {

	enablePower();	

	bool success;

	success = initiateReading();

	if (!success) Serial.println("initiateReading failed");

	if (success) {
		if (useInterrupts) {
			success = readRawDataWithInterrupts();
		}
		else {
			success = readRawDataWithPolling();
		}

		if (!success) Serial.println("readRawData failed");
	}
	
	disablePower();

	Serial.print("data: ");
	Serial.print(data[0]);
	Serial.print(" ");
	Serial.print(data[1]);
	Serial.print(" ");
	Serial.print(data[2]);
	Serial.print(" ");
	Serial.print(data[3]);
	Serial.print(" ");
	Serial.println(data[4]);
 	
	if (success) {
		success = computeResults(temp, humi);
	}
	return success;
  }
  
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
  
private:

  // turn on A pin to power up sensor
  inline void enablePower(void) {
	mode2(OUTPUT);
	digiWrite2(HIGH);
	
	// Set data pin as an output initially for signalling sensor to take a reading.
	// Start with output high.
	mode(OUTPUT);
	digiWrite(HIGH);
	
	// sensor needs 1 sec to stabilise after power is applied
	Sleepy::loseSomeTime(1000);
  }
  
  inline void disablePower(void) {
	// turn off pullup resistor to save power
	mode(INPUT);
	
	// turn off A pin
	digiWrite2(LOW);
  }
  
  inline bool initiateReading(void) {
	// pull bus low for >18ms to send start signal to sensor
	// Sleepy uses watchdog timer, which has 16ms resolution
	digiWrite(LOW);
	Sleepy::loseSomeTime(32);

	// set bus back to high to indicate we're ready to receive the result
	digiWrite(HIGH);

	// sensor starts its response by pulling bus low within 40us
	// want to wait for it to have gone low before starting the reading
	delayMicroseconds(40);
	
	// enable pullup resistor for the input in case no sensor's connected -
	// it will be easier to detect an unchanging input if it's pulled up
	// rather than floating.
	mode(INPUT_PULLUP);

	// sensor should have pulled line low by now
	if (digiRead() != LOW) {
		// looks like sensor's not responding
		return false;
	}

	return true;
  }

  inline bool readRawDataWithInterrupts(void) {
	bool result = true;

	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

	acquiring = true;
	acquiredSuccessfully = false;
	currentBit = 0;
	dataPin = digiPin();
	pulseStartTime = micros();

	// enable interrupt
	#if defined(__AVR_ATtiny84__)
		bitSet(PCMSK0, portNum * 2 - 2); // port1 = PCINT0, port2 = PCINT2
		bitSet(GIMSK, PCIE0);
	#else
		bitSet(PCMSK2, digiPin());
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
		bitClear(PCMSK2, digiPin());
	#endif
	
	return acquiredSuccessfully;
  }
  
  inline bool readRawDataWithPolling(void) {
  
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
				if (digiRead() != inputLevel) {
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
volatile uint32_t DHT::pulseStartTime;
volatile bool DHT::acquiring;
volatile bool DHT::acquiredSuccessfully;
volatile uint8_t DHT::dataPin;



#endif
