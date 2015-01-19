#ifndef HEATHACK_H
#define HEATHACK_H

#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

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
        UNDEFINED   = 0,
        TEMPERATURE = 1,
        HUMIDITY    = 2,
        LIGHT       = 3,
        MOTION      = 4,
        PRESSURE    = 5,
        SOUND       = 6,
        LOW_BATT    = 7
    };
}
 
extern char* HHSensorTypeNames[];
 
 
struct HHReading {
	byte sensorType : 4;
	byte sensorNumber : 4;
	int16_t reading;
	
	inline void clear() {
		sensorType = HHSensorType::UNDEFINED;
		sensorNumber = 0;
		reading = 0;
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
	
	// add a sensor reading into the packet
	bool addReading(byte sensorNumber, HHSensorType::type type, int16_t value) {
	
		if (numReadings < HH_MAX_READINGS) {
		
			readings[numReadings].sensorType = type;
			readings[numReadings].sensorNumber = sensorNumber;
			readings[numReadings].reading = value;
			
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
