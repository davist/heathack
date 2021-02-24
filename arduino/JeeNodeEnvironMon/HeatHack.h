#ifndef HEATHACK_H
#define HEATHACK_H

#include <Arduino.h>
#include <JeeLib.h>

// Update version number for every release.
// Version number is major.minor
// A change to the major indicates a break in compatibility between transmitter and receiver,
// i.e. all devices on a network need the same major number to work together correctly.
// Minor number is increased for non-breaking changes.
#define VERSION "1.3"


// The serial port baud rate to use for all HeatHack devices
// JNMicro only supports 9600, 38400, or 115200
#define BAUD_RATE 9600

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

/**
 * EEPROM addresses for storing settings
 * Note Jeelib's RF12 uses 0x20 to 0x3f for settings and
 * 0x40 to 0x4f for an encryption key
 * Group and Node id are shared with Jeelib, the rest are specific to Heathack.
 */
#define HH_EEPROM_BASE ((uint8_t*) 0x60)

#define EEPROM_GROUP    (RF12_EEPROM_ADDR + 0) // group id
#define EEPROM_NODE     (RF12_EEPROM_ADDR + 1) // node id
#define EEPROM_INTERVAL (HH_EEPROM_BASE + 0)   // transmit interval in 10s of secs
#define EEPROM_FLAGS    (HH_EEPROM_BASE + 1)   // flags (see below)
#define EEPROM_PORT1    (HH_EEPROM_BASE + 2)   // type of sensor attached to port 1
#define EEPROM_PORT2    (HH_EEPROM_BASE + 3)   // type of sensor attached to port 2
#define EEPROM_PORT3    (HH_EEPROM_BASE + 4)   // type of sensor attached to port 3
#define EEPROM_PORT4    (HH_EEPROM_BASE + 5)   // type of sensor attached to port 4

// flags stored in EEPROM_FLAGS
#define FLAG_ACK 0x01
#define FLAG_VERBOSE 0x02

// per-port sensor types
#define SENSOR_NONE  1   // no sensor attached
#define SENSOR_AUTO  2   // auto-detect DHT11/22, DS18B20, Room Board with HYT131, LCD display, (or none)
#define SENSOR_I2C_UNKNOWN 127
#define SENSOR_DHT11 11
#define SENSOR_DHT22 22
#define SENSOR_DS18B 18
#define SENSOR_HYT131 131
#define SENSOR_LCD   5
#define SENSOR_LDR   3   // light-dependent resistor between AIO and GND pins
#define SENSOR_PULSE 4   // pulsed input on DIO pin (e.g. hall-effect switch or photo-detector for meter reading)
#define SENSOR_RTC   6   // Real time clock

// max sensor type number that can be entered in config console
#define SENSOR_MIN 1
#define SENSOR_MAX 4

#define RECEIVER_NODE_ID 1

/**
 * Default values and ranges for eeprom data
 */
// Standard JeeNode group id is 212
#ifndef DEFAULT_GROUP_ID
#define DEFAULT_GROUP_ID 212
#endif

// Node id 2 - 30 (1 reserved for receiver)
#ifndef DEFAULT_NODE_ID
#define DEFAULT_NODE_ID 2
#endif

// How frequently readings are sent normally, in 10s of seconds
#ifndef DEFAULT_INTERVAL
#define DEFAULT_INTERVAL 1
#endif

// min and max

#ifndef GROUP_MIN
#define GROUP_MIN 200
#endif

#ifndef GROUP_MAX
#define GROUP_MAX 212
#endif

#ifndef NODE_MIN
#define NODE_MIN 2
#endif

#ifndef NODE_MAX
#define NODE_MAX 30
#endif

// 10 secs min
#ifndef INTERVAL_MIN
#define INTERVAL_MIN 1
#endif

// 42.5 mins max
#ifndef INTERVAL_MAX
#define INTERVAL_MAX 255
#endif

/**
 * Acknowledgement and retry settings
 */
#define ACK_TIME        10  // number of milliseconds to wait for an ack
#define RETRY_PERIOD    1000  // how soon to retry if ACK didn't come in
#define RETRY_LIMIT     5   // maximum number of times to retry
#define SUCCESSIVE_RETRY_THRESHOLD 4 // number of doReports that must fail on 1st try before recalcing min transmit power
#define NO_RESPONSE     255  // value returned by findMinTransmitPower indicating receiver couldn't be contacted
#define POWER_RETRY_PERIOD    100  // how soon to retry when determining min transmit power

// set the sync mode to 2 if the fuses are still the Arduino default
// mode 3 (full powerdown) can only be used with 258 CK startup fuses
#define RADIO_SYNC_MODE 2

// To avoid wasting power sending frequent readings when the receiver isn't contactable,
// the node will switch to a less-frequent mode when it doesn't get acknowledgments from
// the receiver. It will switch back to normal mode as soon as an ack is received.
#define MAX_SECS_WITHOUT_ACK (30 * 60)  // how long to go without an acknowledgement before switching to less-frequent mode
#define SECS_BETWEEN_TRANSMITS_NO_ACK 600  // how often to send readings in the less-frequent mode

/**
 * LCD screen dimensions
 */
#define LCD_WIDTH 16
#define LCD_HEIGHT 2

/**
 * Definitions for the data packet
 */
 
// Sensor types
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
 
extern const char* HHSensorTypeNames[];
extern const char* HHSensorUnitNames[];
extern bool HHSensorTypeIsInt[]; 
 
struct HHReading {
	
	union {
		struct {
      byte sensorType : 4;
			byte portNumber : 2;
			byte sensorNumber : 2;
		};
		
		byte header;
	};
	
	int16_t encodedReading;

	// port and sensor are 1-based to match JeeNode labelling, but transmitted zero-based
	// to fit into the available bits
  uint8_t getPort() {
    return portNumber + 1;
  }

  void setPort(uint8_t p) {
    portNumber = p - 1;
  }

  uint8_t getSensor() {
    return sensorNumber + 1;
  }

  void setSensor(uint8_t s) {
    sensorNumber = s - 1;
  }

	int16_t getIntPartOfReading() {
		if (HHSensorTypeIsInt[sensorType]) {
			return encodedReading;
		}
		else {
			return encodedReading / 10;
		}
	}

	uint8_t getDecPartOfReading() {
		if (HHSensorTypeIsInt[sensorType]) {
			return NO_DECIMAL;
		}
		else {
			return abs(encodedReading % 10);
		}
	}
};

// maximum number of readings that will fit in a data packet
#define HH_MAX_READINGS 13

struct HeatHackData {
	//bool isRetransmit : 1;
	byte numReadings : 5;
	byte sequence : 3;
	HHReading readings[HH_MAX_READINGS];

	// clear all previous readings
	void clear() {
		numReadings = 0;
		//isRetransmit = false;
		sequence++;
	}
/*	
	// Add a sensor reading into the packet.
	// Assumes decimal readings (to 1 decimal place) have been multiplied by 10
	// to convert to integer.
	bool addReading(byte portNumber, byte sensorNumber, HHSensorType::type type, int16_t value) {
	
		if (numReadings < HH_MAX_READINGS) {
			readings[numReadings].sensorType = type;
			readings[numReadings].portNumber = portNumber;
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
*/	

	void addReading(HHReading& reading) {
		if (numReadings < HH_MAX_READINGS) {
			readings[numReadings].header = reading.header;
			readings[numReadings].encodedReading = reading.encodedReading;			
			numReadings++;
		}	
	}

	// calculate actual number of bytes in use so we can transmit the minimum
	// possible instead of the entire struct
	byte getTransmitSize() {
		return sizeof(byte) + sizeof(HHReading) * numReadings;
	}	
};

#endif
