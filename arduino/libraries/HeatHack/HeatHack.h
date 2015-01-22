#ifndef HEATHACK_H
#define HEATHACK_H

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

// The serial port baud rate to use for all HeatHack devices
#define BAUD_RATE 57600

/**
 * Pin numbers for the JeeNode ports
 *
 * PORTx_DIO        is the D pin (digital input/output)
 * PORTx_AIO        is the A pin (analogue i/o)
 * PORTx_AIO_AS_DIO is the A pin used as a 2nd digital pin
 * PORT_IRQ_AS_DIO  is the I pin used as a digital pin (note no port num as it's shared across all ports)
 * PORT_IRQ         is the interrupt number for the I pin
 */
#if defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__)

#define PORT1_DIO 0

#define PORT1_AIO_AS_DIO 2

#define PORT1_AIO 0

#define PORT_IRQ_AS_DIO 1

#elif defined(__AVR_ATtiny84__)

// JeeNode Micro - only 2 ports

#define PORT1_DIO 10
#define PORT2_DIO 8

#define PORT1_AIO_AS_DIO 9
#define PORT2_AIO_AS_DIO 7

#define PORT1_AIO 9
#define PORT2_AIO 7

#define PORT_IRQ_AS_DIO 3

#else

// Standard JeeNode with 4 ports

#define PORT1_DIO 4
#define PORT2_DIO 5
#define PORT3_DIO 6
#define PORT4_DIO 7

#define PORT1_AIO_AS_DIO 14
#define PORT2_AIO_AS_DIO 15
#define PORT3_AIO_AS_DIO 16
#define PORT4_AIO_AS_DIO 17

#define PORT1_AIO 0
#define PORT2_AIO 1
#define PORT3_AIO 2
#define PORT4_AIO 3

#define PORT_IRQ_AS_DIO 3

#endif

#define PORT_IRQ 1

// sensor types
namespace HHSensorType {
    enum type {
        TEST        = 0,	// for sending test data. Should not be logged
        TEMPERATURE = 1,	// degrees celcius as a decimal to 0.1 degrees
        HUMIDITY    = 2,	// percentage as a decimal to 0.1 degrees
        LIGHT       = 3,	// int value, 0 (darkest) to 255 (brightest)
        MOTION      = 4,	// integer count of motion events since last transmit
        PRESSURE    = 5,	// decimal value, as yet undefined but expect millibars
        SOUND       = 6,	// decimal value, as yet undefined but expect dB
        LOW_BATT    = 7		// int value, 0 - battery OK, 1 - low battery
    };
}

#define NO_DECIMAL 255
 
extern char* HHSensorTypeNames[];
extern bool HHSensorTypeIsInt[]; 
 
struct HHReading {
	byte sensorType : 4;
	byte sensorNumber : 4;
	int16_t encodedReading;
	
	inline void clear() {
		sensorType = HHSensorType::TEST;
		sensorNumber = 0;
		encodedReading = 0;
	}
	
	inline int16_t getIntPartOfReading() {
		if (HHSensorTypeIsInt[sensorType]) {
			return encodedReading;
		}
		else {
			return encodedReading / 10;
		}
	}

	inline uint8_t getDecPartOfReading() {
		if (HHSensorTypeIsInt[sensorType]) {
			return NO_DECIMAL;
		}
		else {
			return encodedReading % 10;
		}
	}
};

// maximum number of readings that will fit in a data packet
#define HH_MAX_READINGS 21

struct HeatHackData {
	byte numReadings : 5;
	byte sequence : 3;
	HHReading readings[HH_MAX_READINGS];

	// clear all previous readings
	void clear() {
		for (byte i = 0; i < numReadings; i++) {
			readings[i].clear();
		}
		numReadings = 0;
		sequence++;
	}
	
	// Add a sensor reading into the packet.
	// Assumes decimal readings (to 1 decimal place) have been multiplied by 10
	// to convert to integer.
	bool addReading(byte sensorNumber, HHSensorType::type type, int16_t value) {
	
		if (numReadings < HH_MAX_READINGS) {
		
			readings[numReadings].sensorType = type;
			readings[numReadings].sensorNumber = sensorNumber;
			readings[numReadings].encodedReading = value;
			
			numReadings++;
			return true;
		}
		else {
			// too many readings
			return false;
		}
	}	
	
	// calculate actual number of bytes in use so we can transmit the minimum
	// possible instead of the entire struct
	byte getTransmitSize() {
		return sizeof(byte) + sizeof(HHReading) * numReadings;
	}	
};


#endif
