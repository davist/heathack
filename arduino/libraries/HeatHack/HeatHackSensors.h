#ifndef HEATHACK_SENSORS_H
#define HEATHACK_SENSORS_H

#include <JeeLib.h>

#define DHT11_TYPE 11
#define DHT22_TYPE 22

#ifndef DHT_USE_INTERRUPTS
	#define DHT_USE_INTERRUPTS true
#endif

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
 * The line: ISR(PCINT2_vect) { DHT::isrCallback(); }
 * must be included in the main sketch for it to work.
 * To use polling instead, add the line: #define DHT_USE_INTERRUPTS false
 *
 * Sleepy is used for delays, therefore ISR(WDT_vect) { Sleepy::watchdogEvent(); }
 * must also be included in the main sketch.
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
  DHT (byte portNum, byte sensorType);
  
  // Results are returned in tenths of a degree and percent, respectively.
  // i.e 25.4 is returned as 254.
  bool reading (int& temp, int& humi);

#if DHT_USE_INTERRUPTS  
  // the interrupt handler
  static void isrCallback();
#endif
  
protected:

  inline void disablePower(void);  
  inline void enablePower(void);  
  inline bool initiateReading(void);
  inline bool readRawData(void);  
  inline bool computeResults(int& temp, int& humi);
};

#endif
