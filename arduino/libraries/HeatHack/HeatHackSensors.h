#ifndef HEATHACK_SENSORS_H
#define HEATHACK_SENSORS_H

#include <jeelib,h>

// time in microseconds to wait after turning on sensor power
#define DS18_POWERUP_TIME 50 

#define DHT11_TYPE 11
#define DHT22_TYPE 22


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
  byte data[6]; // holds a few start bits and then the 5 real payload bytes
  
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

	enablePower();	
	initiateReading();

	bool success = readRawData();
	
	sei();
	disablePower();

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
  
	// each bit is a high edge followed by a variable length low edge
	for (byte i = 7; i < 48; ++i) {

	// wait for the high edge, then measure time until the low edge
		byte timer;
		for (byte j = 0; j < 2; ++j) {
		  for (timer = 0; timer < 250; ++timer) {
			if (digiRead() != j) break;
		  }
		}

		// if no transition was seen, return 
		if (timer >= 250) return false;
		
		// collect each bit in the data buffer
		byte offset = i / 8;
		data[offset] <<= 1;
		
		// short pulse is a 0, long pulse a 1
		data[offset] |= timer > 7;
	}

	return true;
  }

  inline bool computeResults(int& temp, int& humi) {
	// calculate checksum to ensure data is valid
	byte sum = data[1] + data[2] + data[3] + data[4];
	if (sum != data[5]) {
		return false;
	}

	word t;

	// higher precision DHT22 uses 2 bytes instead of 1
	if (type == DHT22_TYPE) {
		humi =  (data[1] << 8) | data[2];
		t = ((data[3] & 0x7F) << 8) | data[4];
	}
	else {
		humi =  10 * data[1];
		t = 10 * data[3];
	}

	// is temperature negative?
	temp = data[3] & 0x80 ? - t : t;
	
	return true;
  }
};




#endif
