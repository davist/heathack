#ifndef HEATHACK_SHARED_H
#define HEATHACK_SHARED_H

#include <Arduino.h>
#include <HeatHack.h>
#include <HeatHackSensors.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

/////////////////////////////////////////////////////////////////////
// Functions and variables shared between standard and micro Jeenodes
/////////////////////////////////////////////////////////////////////



// variables populated from EEPROM settings
static uint8_t myNodeID;       // node ID used for this unit
static uint8_t myGroupID;      // group ID used for this unit
static uint8_t myInterval;     // interval between transmissions in 10s of secs

#if !defined(__AVR_ATtiny84__)
// extended eeprom data that's not used by the Micro

static uint8_t portSensor[4];  // type of sensor attached to each port
static uint8_t eepromFlags;
#endif


// interrupt handler for the Sleepy watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

// time we last received an acknowledgement from the receiver
static uint32_t lastAckTime = 0;

// if not received an ack for a while, hibernating will be true
static bool hibernating = false;

static HeatHackData dataPacket;

// default to maximum power
static uint8_t transmitPower = 0;

// count number of successive times a doReport fails on the initial try and has to retry.
// If it happens several times then recalc min transmit power
static uint8_t successiveRetries = 0;

/////////////////////////////////////////////////////////////////////
// flash the ACT LED on the JeeNode SMD and USB boards.
void flashLED(uint8_t numFlashes = 1) {
#if !defined(__AVR_ATtiny84__)
  pinMode(9, OUTPUT);

  while (numFlashes > 0) {
    digitalWrite(9, LOW);
    delay(1);
    digitalWrite(9, HIGH);

    numFlashes--;

    // delay between flashes
    if (numFlashes > 0) Sleepy::loseSomeTime(100);
  }
#endif
}

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
  Serial.flush();
  delay(10); // make sure tx buf is empty before going back to sleep
}


/////////////////////////////////////////////////////////////////////
// sends a test packet and waits for an ack while increasing the
// transmit power. Returns min level (7-0) at which a response was received,
// or -1 if no response received at any power level.
uint8_t findMinTransmitPower(void) {

	uint8_t txPower = 7;
	uint8_t ackCount = 0;

  HHReading reading;
  reading.sensorType = HHSensorType::TEST;
  dataPacket.clear();
  dataPacket.addReading(reading);

#if DEBUG
  Serial.println(F("Finding min transmit power (0=max)"));
  Serial.flush();
#endif
	
  do {
		// try twice at each power level to check for consistency
		ackCount = 0;
		for (uint8_t tries = 2; tries > 0; tries--) {

	    rf12_sleep(RF12_WAKEUP);
	    rf12_control(0x9850 | txPower); // set radio's transmit power
      rf12_sendNow(RF12_HDR_ACK, &dataPacket, dataPacket.getTransmitSize());
			rf12_sendWait(RADIO_SYNC_MODE);
			if (waitForAck()) ackCount++;
	    rf12_sleep(RF12_SLEEP);

      #if DEBUG
        Serial.print(F("  power "));
        Serial.print(txPower);
        Serial.print(F(" try "));
        Serial.print(3 - tries);
        Serial.print(F(" acks "));
        Serial.println(ackCount);
        Serial.flush();
      #endif

      Sleepy::loseSomeTime(POWER_RETRY_PERIOD);
		}

		if (ackCount < 2) {
		  // didn't get an ack for every transmit
		  txPower--;
		}
	}
  while (txPower >= 0 && txPower <= 7 && ackCount < 2);


#if DEBUG
  if (txPower == NO_RESPONSE) Serial.println(F("No response from receiver"));
  else {
    Serial.print(F("Using power "));
    Serial.println(txPower);
  }
  Serial.flush();
#endif

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
#if !defined(__AVR_ATtiny84__)
  if (eepromFlags & FLAG_VERBOSE) {
      Serial.println(F("--------------------------"));

      for (byte i=0; i<dataPacket.numReadings; i++) {
        Serial.print(F("* sensor "));
        Serial.print(dataPacket.readings[i].getPort());
        Serial.print(dataPacket.readings[i].getSensor());
        Serial.print(F(": "));
        Serial.print(HHSensorTypeNames[dataPacket.readings[i].sensorType]);
        Serial.print(F(" "));
        Serial.print(dataPacket.readings[i].getIntPartOfReading());

        uint8_t decimal = dataPacket.readings[i].getDecPartOfReading();
        if (decimal != NO_DECIMAL) {
          // display as decimal value to 1 decimal place
          Serial.print(F("."));
          Serial.print(decimal);
        }
        Serial.println();
      }
      serialFlush();
  }
#endif

  if (dataPacket.numReadings == 0) {
    // nothing to do
    return;
  }

  bool acked = false;
  byte retry = 0;

  do {
    if (retry == 0) {
      setTransmitPower(transmitPower);
    }
    else {
#if DEBUG
      Serial.println("No Ack received. Resending at max power.");
      Serial.flush();
#endif

      // use maximum power for retries
      setMaxTransmitPower();

      // delay before any retry
      Sleepy::loseSomeTime(RETRY_PERIOD);
  //dataPacket.isRetransmit = true;
    }

    flashLED();

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

  if (acked && retry == 1) {
    // succeeded on first try
    successiveRetries = 0;
  }
  else {
    successiveRetries++;
  }

  if (successiveRetries > SUCCESSIVE_RETRY_THRESHOLD) {
    transmitPower = findMinTransmitPower();
    successiveRetries = 0;
  }
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
    Serial.print(F("hibernating: "));
    Serial.println(hibernating ? F("true") : F("false"));
    Serial.print(F("delay(ms): "));
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
      Serial.print(F("slept for (ms): "));
      Serial.println(sleepMs);
      Serial.print(F("new delay(ms): "));
      Serial.println(delayMs);
      serialFlush();
    #endif
  }
  // loseSomeTime has minimum resolution of 16ms
  while (delayMs > 16);
  
  #if DEBUG
    Serial.println(F("finished sleeping"));
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
	eepromFlags = eeprom_read_byte(EEPROM_FLAGS);
	
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
	eeprom_update_byte(EEPROM_FLAGS, eepromFlags);
  #endif
}


#if !defined(__AVR_ATtiny84__)

/////////////////////////////////////////////////////////////////////
inline uint8_t readline(char *buffer, uint8_t bufSize)
{
  static uint8_t pos = 0;

  int readch = Serial.read();

  if (readch > 0) {
	uint8_t len;
    switch (readch) {
      case '\n': // Ignore new-lines
        break;
      case '\r': // Return on CR
        len = pos;
		pos = 0;  // Reset position index ready for next time
        return len;
      default:
        Serial.print(((char)readch));
        if (pos < bufSize-1) {
          buffer[pos++] = readch;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found.
  return 0;
}

/////////////////////////////////////////////////////////////////////
inline uint16_t parseInt(char* buf, uint16_t min, uint16_t max) {
  uint16_t result = 0;
  char *cur = buf;
  
  // find end of number (marked by end of string or space)
  while (*cur && *cur != ' ') cur++;
  cur--;
  
  // parse from end to start
  uint8_t order = 1;
  while (cur >= buf) {
    char c = *cur;
    if (c >= '0' && c <= '9') result += (c - '0') * order;
    cur--;
    order *= 10;
  }

  if (result < min) result = min;
  else if (result > max) result = max;
  
  return result;
}

/////////////////////////////////////////////////////////////////////
void displaySettings(void) {
	Serial.println();
	Serial.println(F("Current settings:"));
	Serial.print(F(" group id "));
	Serial.println(myGroupID);
	Serial.print(F(" node id "));
	
	#if RECEIVER_NODE
		Serial.print(RECEIVER_NODE_ID);
		Serial.println(F(" (fixed for receiver)"));
	
		Serial.print(F(" acknowledgements "));
		Serial.println( (eepromFlags & FLAG_ACK) ? "on" : "off" );
  #else
		Serial.println(myNodeID);
		Serial.print(F(" transmit interval "));
		Serial.print(myInterval);
		Serial.println(F("0 seconds"));
/*
		for (uint8_t port = 0; port <= 3; port++ ) {
			Serial.print(F(" port "));
			Serial.print(port + 1);
			Serial.print(F(" "));
			switch (portSensor[port]) {

			case SENSOR_NONE:
				Serial.println(F("disabled"));
				break;
			case SENSOR_AUTO:
				Serial.println(F("auto"));
				break;
			case SENSOR_LDR:
				Serial.println(F("LDR (light sensor)"));
				break;
			case SENSOR_PULSE:
				Serial.println(F("pulse"));
				break;
			default:
				Serial.println(F("[invalid value]"));
				break;					
			}
		}
*/

    #if DHT_PORT
		  Serial.print(F(" DHT11/22 on port "));
      Serial.println(DHT_PORT);
    #endif

    #if DS18B_PORT
      Serial.print(F(" DS18B on port "));
      Serial.println(DS18B_PORT);
    #endif

    #if HYT131_PORT
      Serial.print(F(" HYT131 (room board) on port "));
      Serial.println(HYT131_PORT);
    #endif

    #if LDR_PORT
      Serial.print(F(" LDR (room board) on port "));
      Serial.println(LDR_PORT);
    #endif

    #if PIR_PORT
      Serial.print(F(" PIR (room board) on port "));
      Serial.println(PIR_PORT);
    #endif

    #if LCD_PORT
      Serial.print(F(" LCD display on port "));
      Serial.println(LCD_PORT);
    #endif

	#endif

  Serial.print(F(" verbose output "));
  Serial.println( (eepromFlags & FLAG_VERBOSE) ? "on" : "off" );
	
	Serial.println();
	Serial.println(F("Commands:"));
	Serial.println(F(" h - show settings and commands (help)"));
	Serial.println(F(" w - write changed settings to EEPROM (make them permanent)"));	
	Serial.println(F(" x - exit config mode"));
	Serial.println(F(" g<nnn> - set group id. Valid values: 200 - 212"));

	#if RECEIVER_NODE
	Serial.println(F(" a<0/1> - turn acks on or off. Valid values: 0 - off, 1 - on"));
	#else
	Serial.println(F(" n<nn> - set node id. Valid values: 2 - 30"));
	Serial.println(F(" i<nnn> - set interval. Valid values: multiples of 10 from 10 to 2550"));
//	Serial.println(F(" p<n> <s> - set port n to sensor type s. Valid values: 1-4 for port,"));
//  Serial.println(F("            sensor: 1 - disabled, 2 - auto, 3 - ldr, 4 - pulse"));
  Serial.println(F(" s - test sensors on all ports"));
//  Serial.println(F(" s - test sensors on all ports and report readings"));
//	Serial.println(F(" s<n> - test sensor on port n and report reading"));
	Serial.println(F(" r - send a test radio packet"));
	#endif

  Serial.println(F(" v<0/1> - turn verbose output on or off. Valid values: 0 - off, 1 - on"));

	Serial.println();
}

/////////////////////////////////////////////////////////////////////
void parseCommand(char *buffer, uint8_t len) {

  switch(buffer[0]) {

	// help
	case 'h':
		displaySettings();
		break;	

		// write
	case 'w':
		writeEeprom();
		break;	

		// group
	case 'g':
		if (len > 1) {
			myGroupID = parseInt(&buffer[1], GROUP_MIN, GROUP_MAX);
			Serial.print(F("Group id set to "));
			Serial.println(myGroupID);
		}
		break;

  // verbose
  case 'v':
    if (len > 1) {
      if (parseInt(&buffer[1], 0, 1) == 1) {
        // set flag
        eepromFlags |= FLAG_VERBOSE;
      }
      else {
        // unset flag
        eepromFlags &= (0xFF - FLAG_VERBOSE);
      }
      Serial.print(F("Verbose output set to "));
      Serial.println( (eepromFlags & FLAG_VERBOSE) ? "on" : "off" );
    }
    break;
	
	#if RECEIVER_NODE

	// acks
	case 'a':
		if (len > 1) {
			if (parseInt(&buffer[1], 0, 1) == 1) {
        // set flag
				eepromFlags |= FLAG_ACK;
			}
			else {
        // unset flag
				eepromFlags &= (0xFF - FLAG_ACK);
			}
			Serial.print(F("Acknowledgements set to "));
			Serial.println( (eepromFlags & FLAG_ACK) ? "on" : "off" );
		}
		break;
	#else
	
	// node
	case 'n':
		if (len > 1) {
			myNodeID = parseInt(&buffer[1], NODE_MIN, NODE_MAX);
			Serial.print(F("Node id set to "));
			Serial.println(myNodeID);
		}
		break;

	// interval
	case 'i':
		if (len > 1) {
			myInterval = parseInt(&buffer[1], INTERVAL_MIN * 10, INTERVAL_MAX * 10) / 10;
			Serial.print(F("Interval set to "));
			Serial.println(myInterval * 10);
		}
		break;

	// port
	case 'p':
		if (len == 4) {
			uint8_t port = parseInt(&buffer[1], 1, 4) - 1;
			uint8_t sensorType = parseInt(&buffer[3], SENSOR_MIN, SENSOR_MAX);
			portSensor[port] = sensorType;
			Serial.print(F("Port "));
			Serial.print(port + 1);
			Serial.print(F(" set to "));
			switch (sensorType) {
			case SENSOR_NONE:
				Serial.println(F("disabled"));
				break;
			case SENSOR_AUTO:
				Serial.println(F("auto"));
				break;
			case SENSOR_LDR:
				Serial.println(F("LDR"));
				break;
			case SENSOR_PULSE:
				Serial.println(F("pulse"));
				break;
			}
		}
		break;
		
	// test sensors
	case 's':
		{
		Autodetect ad;
		for (byte p=1; p<=4; p++) {
			Serial.print(F("Port "));
			Serial.print(p);
			Serial.print(F(": "));
			serialFlush();
			switch(ad.probePort(p)) {
				case SENSOR_DHT11:
					Serial.println(F("DHT11"));
					break;
				case SENSOR_DHT22:
					Serial.println(F("DHT22"));
					break;
				case SENSOR_DS18B:
					Serial.println(F("DS18B20"));
					break;
				case SENSOR_HYT131:
					Serial.println(F("HYT131"));
					break;
				case SENSOR_LCD:
					Serial.println(F("LCD"));
					break;
				default:
					Serial.println(F("None"));
			}
		}
	
		break;
		}

	case 'r':
	  {
	  Serial.flush();
    uint8_t power = findMinTransmitPower();
    if (power == NO_RESPONSE) {
      Serial.println(F("No response from receiver"));
    }
    else {
      Serial.print(F("Got response at power level "));
      Serial.print(power);
      Serial.println(F(" (0 max, 7 min)"));
    }

    break;
    }

	#endif
	}
}

/////////////////////////////////////////////////////////////////////
// config console for standard Jeenodes
void configConsole(void) {

	// wait and see if user wants to enter config
	Serial.println();
	Serial.println(F("Press 'c' to enter config mode, any other key to skip"));
	serialFlush();
	
	uint32_t start = millis();
	bool enterConfig = false;
	
	for (uint8_t secs=5; secs > 0; secs--) {
		Serial.print(F("\r"));
		Serial.print(secs);
    serialFlush();
    flashLED();
		while (millis() - start < 1000) {
			int key = Serial.read();
			if (key > 0) {
				if (key == 'c') enterConfig = true;
				secs = 1;
				break;
			}
		}
		start = millis();
	}
	
	if (enterConfig) {
		// real config
		
		// clear any excess chars in input buffer
		while (Serial.read() > 0);
		
		displaySettings();

		char buffer[10];
		uint8_t len;
		
		while (buffer[0] != 'x') {
			Serial.print(F("->"));
			serialFlush();

			// wait for a command
			do {
				len = readline(buffer, 10);
			} while (len == 0);
			Serial.println();
			
			parseCommand(buffer, len);
		}
	}
	
	Serial.println();
	Serial.println(F("Entering run mode"));
  serialFlush();
  flashLED(2);
}
#endif

#endif

