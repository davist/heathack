#ifndef HEATHACK_SHARED_H
#define HEATHACK_SHARED_H

#include <HeatHack.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

/////////////////////////////////////////////////////////////////////
// Functions and variables shared between standard and micro Jeenodes
/////////////////////////////////////////////////////////////////////

// variables populated from EEPROM settings
uint8_t myNodeID;       // node ID used for this unit
uint8_t myGroupID;      // group ID used for this unit
uint8_t myInterval;     // interval between transmissions in 10s of secs

#if !defined(__AVR_ATtiny84__)
// extended eeprom data that's not used by the Micro

uint8_t portSensor[4];  // type of sensor attached to each port
uint8_t receiverFlags;
#endif


// interrupt handler for the Sleepy watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

// time we last received an acknowledgement from the receiver
uint32_t lastAckTime = 0;

// if not received an ack for a while, hibernating will be true
bool hibernating = false;

HeatHackData dataPacket;



/////////////////////////////////////////////////////////////////////
// wait a few milliseconds for proper ACK to me, return true if indeed received
bool waitForAck(void) {
    uint32_t ackTimer = millis();

    do {
        if (rf12_recvDone() &&
            rf12_crc == 0 &&
            rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID)) {
              
          // see http://talk.jeelabs.net/topic/811#post-4712
    
          lastAckTime = millis();
          return true;
        }
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_mode();
    }
    while (millis() - ackTimer < ACK_TIME);
    
    return false;
}


/////////////////////////////////////////////////////////////////////
void serialFlush (void) {
    #if ARDUINO >= 100
        Serial.flush();
    #endif  
    delay(10); // make sure tx buf is empty before going back to sleep
}


/////////////////////////////////////////////////////////////////////
// sends a test packet and waits for an ack while increasing the
// transmit power. Returns min level (7-0) at which a response was received.
uint8_t findMinTransmitPower(void) {

	uint8_t txPower;
	bool gotAck = false;

	HeatHackData testPacket;
	testPacket.addReading(0, HHSensorType::TEST, 0);
	
    rf12_sleep(RF12_WAKEUP);
	
	for (txPower = 7; txPower >= 0 && !gotAck; txPower--) {

		// set radio's transmit power
		rf12_control(0x9850 | txPower);

		// try twice at each power level
		for (uint8_t tries = 2, gotAck = false; tries > 0 && !gotAck; tries--) {		
			rf12_sendNow(RF12_HDR_ACK, &testPacket, testPacket.getTransmitSize());
			rf12_sendWait(RADIO_SYNC_MODE);
			gotAck = waitForAck();
		}
	}

    rf12_sleep(RF12_SLEEP);
		
	return txPower;
}

/////////////////////////////////////////////////////////////////////
// sets radio transmit power from 0 max to 7 min
inline void setTransmitPower(uint8_t power) {
	rf12_control(0x9850 | (power & 7));
}

/////////////////////////////////////////////////////////////////////
// set radio transmit power to maximum (level 0)
inline void setMaxTransmitPower(void) {
	rf12_control(0x9850);
}


/////////////////////////////////////////////////////////////////////
// periodic report, i.e. send out a packet and optionally report on serial port
inline void doReport(void) {
    #if DEBUG  
        Serial.println("--------------------------");

        for (byte i=0; i<dataPacket.numReadings; i++) {
          Serial.print("* sensor ");
          Serial.print(dataPacket.readings[i].sensorNumber);
          Serial.print(": ");
          Serial.print(HHSensorTypeNames[dataPacket.readings[i].sensorType]);
          Serial.print(" ");
          Serial.print(dataPacket.readings[i].getIntPartOfReading());
          
          uint8_t decimal = dataPacket.readings[i].getDecPartOfReading();        
          if (decimal != NO_DECIMAL) {
            // display as decimal value to 1 decimal place
            Serial.print(".");
            Serial.print(decimal);
          }
          Serial.println();        
        }
        serialFlush();
    #endif

    if (dataPacket.numReadings == 0) {
      // nothing to do
      return;
    }
    
    bool acked = false;
    byte retry = 0;

    do {
      if (retry != 0) {
        // delay before any retry
        Sleepy::loseSomeTime(RETRY_PERIOD);
		dataPacket.isRetransmit = true;
      }
      
      // send the data and wait for an acknowledgement
      rf12_sleep(RF12_WAKEUP);
      rf12_sendNow(RF12_HDR_ACK, &dataPacket, dataPacket.getTransmitSize());
      rf12_sendWait(RADIO_SYNC_MODE);
      acked = waitForAck();
      rf12_sleep(RF12_SLEEP);
      
      retry++;
    }
    // if hibernating only send once, otherwise keep resending until ack received or retry limit reached
    while (!acked && !hibernating && retry < RETRY_LIMIT);
}


/////////////////////////////////////////////////////////////////////
inline void doSleep(void) {
  // if too long without ack, switch to hibernation mode
  if ((millis() - lastAckTime) > ((uint32_t)MAX_SECS_WITHOUT_ACK) * 1000) {
    hibernating = true;
  }
  
  // calculate time till next measure and report
  uint32_t delayMs;
  if (hibernating) {  
    delayMs = ((uint32_t)SECS_BETWEEN_TRANSMITS_NO_ACK) * 1000;
  }
  else {
    delayMs = ((uint32_t)myInterval) * 10000;
  }

  #if DEBUG
    Serial.print("hibernating: ");
    Serial.println(hibernating ? "true" : "false");
    Serial.print("delay(ms): ");
    Serial.println(delayMs);
    serialFlush();
  #endif

  // wait for delayMs milliseconds
  do {
    uint32_t startTime = millis();
  
    // loseSomeTime is 60 secs max, so go to sleep for up to 60 secs
    Sleepy::loseSomeTime(delayMs <= 60000 ? delayMs : 60000);
    
    // see how long we really slept (in case an interrupt woke us early)
    uint32_t sleepMs = millis() - startTime;

    if (sleepMs < delayMs) {
      delayMs -= sleepMs;
    }
    else {
      delayMs = 0;
    }

    #if DEBUG
      Serial.print("slept for (ms): ");
      Serial.println(sleepMs);
      Serial.print("new delay(ms): ");
      Serial.println(delayMs);
      serialFlush();
    #endif
  }
  // loseSomeTime has minimum resolution of 16ms
  while (delayMs > 16);
  
  #if DEBUG
    Serial.println("finished sleeping");
    serialFlush();
  #endif  
}


/////////////////////////////////////////////////////////////////////
inline void readEeprom(void) {
  myGroupID = eeprom_read_byte(EEPROM_GROUP);
  myNodeID = eeprom_read_byte(EEPROM_NODE);
  myInterval = eeprom_read_byte(EEPROM_INTERVAL); // interval stored in 10s of seconds
  
  // validate the values in case they've never been initialised or have been corrupted
  if (myGroupID < GROUP_MIN || myGroupID > GROUP_MAX) myGroupID= DEFAULT_GROUP_ID;
  if (myNodeID < NODE_MIN || myNodeID > NODE_MAX) myNodeID= DEFAULT_NODE_ID;
  if (myInterval < INTERVAL_MIN || myInterval > INTERVAL_MAX) myInterval= DEFAULT_INTERVAL;
  
  #if !defined(__AVR_ATtiny84__)
	// extended eeprom data that's not used by the Micro
	portSensor[0] = eeprom_read_byte(EEPROM_PORT1);
	portSensor[1] = eeprom_read_byte(EEPROM_PORT2);
	portSensor[2] = eeprom_read_byte(EEPROM_PORT3);
	portSensor[3] = eeprom_read_byte(EEPROM_PORT4);
	receiverFlags = eeprom_read_byte(EEPROM_RX_FLAGS);
	
	for (uint8_t i=0; i<=3; i++) {
		if (portSensor[i] < SENSOR_MIN || portSensor[i] > SENSOR_MAX) portSensor[i] = SENSOR_AUTO;
	}
  #endif
}

/////////////////////////////////////////////////////////////////////
inline void writeEeprom(void) {
  // only write values that have changed to avoid excessive wear on the memory
  eeprom_update_byte(EEPROM_GROUP, myGroupID);
  eeprom_update_byte(EEPROM_NODE, myNodeID);
  eeprom_update_byte(EEPROM_INTERVAL, myInterval);
  
  #if !defined(__AVR_ATtiny84__)
	// extended eeprom data that's not used by the Micro
	eeprom_update_byte(EEPROM_PORT1, portSensor[0]);
	eeprom_update_byte(EEPROM_PORT2, portSensor[1]);
	eeprom_update_byte(EEPROM_PORT3, portSensor[2]);
	eeprom_update_byte(EEPROM_PORT4, portSensor[3]);
	eeprom_update_byte(EEPROM_RX_FLAGS, receiverFlags);
  #endif
}



#endif

