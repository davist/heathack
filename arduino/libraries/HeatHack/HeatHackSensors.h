#ifndef HEATHACK_SENSORS_H
#define HEATHACK_SENSORS_H

#include <jeelib,h>

// time in microseconds to wait after turning on sensor power
#define DS18_POWERUP_TIME 50 

#define DHT11_TYPE 11
#define DHT22_TYPE 22

// Counting pulse lengths depends on processor clock speed.
// Assumption here is that the tiny84 (JNMicro) is at 8MHz, otherwise 16MHz.
#if defined(__AVR_ATtiny84__)
	#define DHT_MAX_TIMER 16
	#define DHT_LONG_PULSE 5
#else
	#define DHT_MAX_TIMER 32
	#define DHT_LONG_PULSE 9
#endif

/* Interface for the DHT11 and DHT22 sensors, does not use floating point.
 * Based on Jeelib's DHTxx class.
 *
 * Sensor's data line is connected to D pin.
 * If sensor's power is connected to A instead of +, it will only be turned on
 * when taking a reading to save power.
 *
 * Sleepy is used for delays, therefore ISR(WDT_vect) { Sleepy::watchdogEvent(); }
 * must be included in the main sketch
 */
class DHT : public Port {
  byte type;
  byte data[5]; // holds the 5 payload bytes
  
public:
  /**
   * portNum: 1, 2, 3 or 4
   * sensorType: DHT11_TYPE or DHT22_TYPE
   */
  DHT (byte portNum, byte sensorType)
	: Port(portNum), type(sensorType) {
  }
  
  // Results are returned in tenths of a degree and percent, respectively.
  // i.e 25.4 is returned as 254.
  bool reading (int& temp, int& humi) {

	data[0] = data[1] = data[2] = data[3] = data[4] = 0;
  
	enablePower();	
	initiateReading();

	bool success = readRawData();
	
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
  
private:

  // turn on A pin to power up sensor
  inline void enablePower(void) {
	mode2(OUTPUT);
	digiWrite2(HIGH);
	mode(OUTPUT);
	digiWrite(HIGH);
	
	// sensor needs 1 sec to stabilise after power is applied
	Sleepy::loseSomeTime(1000);
  }
  
  inline void disablePower(void) {
	digiWrite2(LOW);
  }
  
  inline void initiateReading(void) {
	  // pull bus low for >18ms to send start signal to sensor
	  // Sleepy uses watchdog timer, which has 16ms resolution
	  digiWrite(LOW);
	  Sleepy::loseSomeTime(32);

	  // disable interrupts while receiving data
	  cli();
	  
	  // set bus back to high to indicate we're ready to receive the result
	  digiWrite(HIGH);
	  
	  // sensor starts its response by pulling bus low within 40us
	  // want to wait for it to have gone low before starting the reading
	  delayMicroseconds(40);
	  mode(INPUT);
  }
  
  inline bool readRawData(void) {
  
	// sensor should have pulled line low by now
	if (digiRead() != LOW) {
		// looks like sensor's not responding
		return false;
	}
  
	bool result = true;
	
	// each bit is a high edge followed by a variable length low edge
	for (byte bit = 0; bit < 41; bit++) {

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
		if (bit >= 1 ) {

			// collect each bit in the data buffer
			byte offset = (bit-1) >> 3; // divide by 8
			data[offset] <<= 1;  // shift existing bits left to make way for next one
			
			// short pulse is a 0, long pulse a 1
			data[offset] |= (timer >= DHT_LONG_PULSE);
		}
	}

	// re-enable interrupts asap
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




#endif
