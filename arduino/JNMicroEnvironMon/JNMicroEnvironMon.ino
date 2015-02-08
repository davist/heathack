/**
 * Default setup is:
 *
 * - DHT11/22 temp/humidty sensor on port one with its data line connected to the I pin.
 * - DS18B temp sensor also on port one with data connected to D.
 *
 * The A pin should be used instead of + for powering both sensors so that power is only
 * switched on when needed.
 *
 * Presence and type of the sensors is detected automatically so no need to reconfigure the
 * code for different setups.
 *
 * Config console
 * --------------
 * The group id, node id and transmit interval can be altered without reprogramming by
 * attaching a BlinkPlug and serial terminal. If a BlinkPlug isn't available, 2 push buttons
 * on a breadboard can be used, or even just 2 wires touched to ground!
 *
 * Remove any sensor plug that's attached to the Micro and fit the BlinkPlug. It may be
 * simpler to fit the BlinkPlug on a breadboard and run connectors to the Micro.
 * Alternatively, connect wires to the DIO, AIO and GND pins. Touch AIO to GND for one button,
 * and DIO to GND for the other.
 * To enable config mode, connect RX to GND.
 *
 * Connect TX on the Micro to RX on the serial adapter (eg USB BUB). Connect +3V and GND
 * to the respective pins on the serial connector (VCC and GND on a BUB).
 *
 * Start up a serial terminal on the PC at 9600 baud then press the reset button on the Micro.
 * Three lines of text should appear:
 * g212 n20 i10
 * Config
 * g212
 *
 * The first line is the current settings: group 212, node 20, interval 10 secs.
 * The last line is the setting being edited. Press one of the buttons to cycle between the
 * settings (group, node, interval and save) and press the other button to change the value.
 * Group goes from 200 to 212, node from 20 to 30 and interval is 10, 60 or 600.
 *
 * To save the values permanently into the EEPROM, select save and then press the other
 * button. It will change to say 'saved' to confirm.
 *
 * Reset the Micro again and check that it shows the correct settings.
 *
 * Remove the serial connection and buttons and reattach the sensors.
 */

// To use debug you will need to disable one of the sections below (DHT, DS18B or CONFIG_CONSOLE support)
// to free up some memory.
// Or run the code on a standard Jeenode to do the debugging.
//#define DEBUG true


// Port number for DHT sensor (must be 1 on JNMicro)
// Comment out to remove the DHT code
#define DHT_PORT 1

// Dallas DS18B port number (must be 1 on JNMicro)
// Comment out to remove the DS18B code
#define DS18B_PORT 1

// Enable config console. Comment out to disable.
#define CONFIG_CONSOLE true

// Standard JeeNode group id is 212
// Config console in this code supports 200 - 212 for simplicity (full range is 0 - 212)
#define DEFAULT_GROUP_ID 212

// Node id 2 - 30 (1 reserved for receiver)
// Config console in this code supports 20 - 30
#define DEFAULT_NODE_ID 20

// How frequently readings are sent normally, in 10s of seconds
// Config console supports 1, 6 and 60 (10secs, 1min, 10mins)
#define DEFAULT_INTERVAL 1

#include <JeeLib.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

// need to include OneWire here as it won't get picked up from inside HeatHackSensors.h
#define ONEWIRE_CRC8_TABLE 0
#include <OneWire.h>

#define DHT_USE_INTERRUPTS false
#define DHT_USE_I_PIN_FOR_DATA true
#include <HeatHackSensors.h>

#include <HeatHack.h>


// To avoid wasting power sending frequent readings when the receiver isn't contactable,
// the node will switch to a less-frequent mode when it doesn't get acknowledgments from
// the receiver. It will switch back to normal mode as soon as an ack is received.
#define MAX_SECS_WITHOUT_ACK (30 * 60)  // how long to go without an acknowledgement before switching to less-frequent mode
#define SECS_BETWEEN_TRANSMITS_NO_ACK 600  // how often to send readings in the less-frequent mode

#define ACK_TIME        10  // number of milliseconds to wait for an ack
#define RETRY_PERIOD    1000  // how soon to retry if ACK didn't come in
#define RETRY_LIMIT     5   // maximum number of times to retry

// set the sync mode to 2 if the fuses are still the Arduino default
// mode 3 (full powerdown) can only be used with 258 CK startup fuses
#define RADIO_SYNC_MODE 2

#if DHT_PORT
  DHT dht(DHT_PORT);
#endif

#if DS18B_PORT
  DS18B ds18b(DS18B_PORT);
  uint8_t ds18bNumDevices;
#endif

HeatHackData dataPacket;

#define SENSOR_LOWBATT  1
#define SENSOR_TEMP     2
#define SENSOR_HUMIDITY 3
#define SENSOR_TEMP2    6

// interrupt handler for the Sleepy watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

// time we last received an acknowledgement from the receiver
uint32_t lastAckTime = 0;

// if not received an ack for a while, hibernating will be true
bool hibernating = false;

enum ConfigMode { GROUP, NODE, INTERVAL, SAVE, NUM_MODES };
enum SaveState { UNSAVED, SAVED };

const uint8_t GROUP_MIN = 200;
const uint8_t GROUP_MAX = 212;

const uint8_t NODE_MIN = 20;
const uint8_t NODE_MAX = 30;

const uint8_t INTERVAL_MIN = 1;  // 10 secs min
const uint8_t INTERVAL_MAX = 60; // 10 mins max

uint8_t myNodeID;       // node ID used for this unit
uint8_t myGroupID;      // group ID used for this unit
uint8_t myInterval;     // interval between transmissions in 10s of secs

uint8_t saveState = UNSAVED;
uint8_t configMode = GROUP;


/////////////////////////////////////////////////////////////////////
// wait a few milliseconds for proper ACK to me, return true if indeed received
bool waitForAck() {
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
void serialFlush () {
    #if ARDUINO >= 100
        Serial.flush();
    #endif  
    delay(10); // make sure tx buf is empty before going back to sleep
}

/////////////////////////////////////////////////////////////////////
// periodic report, i.e. send out a packet and optionally report on serial port
inline void doReport() {
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
inline void doMeasure() {
  static bool firstMeasure = true;
  
  // format data packet
  dataPacket.clear();

  if (rf12_lowbat()) {
    dataPacket.addReading(SENSOR_LOWBATT, HHSensorType::LOW_BATT, 1);
  }
  else if (firstMeasure) {
    // send a reading first time even if battery isn't low so that the logger
    // knows about the sensor and can graph it for when it does start reporting a low batt.
    dataPacket.addReading(SENSOR_LOWBATT, HHSensorType::LOW_BATT, 0);
  }

#if DHT_PORT
  int humi, temp;

  if (dht.reading(temp, humi)) {
    dataPacket.addReading(SENSOR_TEMP, HHSensorType::TEMPERATURE, temp);
    dataPacket.addReading(SENSOR_HUMIDITY, HHSensorType::HUMIDITY, humi);
  }
#endif

#if DS18B_PORT

  int16_t temps[3];

  if (ds18bNumDevices > 0) {
    ds18b.reading(temps);
    
    for (uint8_t i=0; i < ds18bNumDevices; i++) {
      if (temps[i] != DS18B_INVALID_TEMP) {
        dataPacket.addReading(SENSOR_TEMP2 + i, HHSensorType::TEMPERATURE, temps[i]);
      }
    }
  }
#endif

  firstMeasure = false;
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
}

/////////////////////////////////////////////////////////////////////
inline void displaySettings(void) {
  Serial.println();
  Serial.print("g");
  Serial.print(myGroupID, DEC);
  Serial.print(" n");
  Serial.print(myNodeID, DEC);
  Serial.print(" i");
  Serial.print(myInterval * 10, DEC);
  Serial.println();
}

/////////////////////////////////////////////////////////////////////
inline void writeEeprom(void) {
  // only write values that have changed to avoid excessive wear on the memory
  eeprom_update_byte(EEPROM_GROUP, myGroupID);
  eeprom_update_byte(EEPROM_NODE, myNodeID);
  eeprom_update_byte(EEPROM_INTERVAL, myInterval);
}

/////////////////////////////////////////////////////////////////////
void displayCurrentSetting() {

  switch (configMode) {
    case GROUP:
      Serial.print("\rg");
      Serial.print(myGroupID, DEC);
      break;
    case NODE:
      Serial.print("\rn");
      Serial.print(myNodeID, DEC);
      break;
    case INTERVAL:
      Serial.print("\ri");
      Serial.print(myInterval * 10, DEC);
      break;
    case SAVE:
      Serial.print("\rsave");
      if (saveState == SAVED) {
        Serial.print("d");
      }
      break;
  }
  
  // blank out any left over text from previous setting
  Serial.print("  ");
}

/////////////////////////////////////////////////////////////////////
void changeCurrentSetting() {
  switch (configMode) {
    case GROUP:
      if (myGroupID < GROUP_MAX) myGroupID++;
      else myGroupID = GROUP_MIN;
      break;
    case NODE:
      if (myNodeID < NODE_MAX) myNodeID++;
      else myNodeID = NODE_MIN;
      break;
    case INTERVAL:
      switch (myInterval) {
        case 1:
          myInterval = 6;
          break;
        case 6:
          myInterval = 60;
          break;
        default:
          myInterval = 1;
          break;
      }
      break;
    case SAVE:
      writeEeprom();
      saveState = SAVED;
      break;
  }
}

/////////////////////////////////////////////////////////////////////
void configConsole(void) {

  // use A and D as button inputs with pull-up enabled
  pinMode(PORT1_DIO, INPUT);
  pinMode(PORT1_AIO_AS_DIO, INPUT);
  digitalWrite(PORT1_DIO, HIGH);
  digitalWrite(PORT1_AIO_AS_DIO, HIGH);
  
  Serial.println("Config");
  displayCurrentSetting();

  byte button1State = LOW;
  byte button2State = LOW;
  uint32_t lastCheckTime = 0;
  const uint32_t DEBOUNCE_TIME = 100;
  
  while (1) {

    uint32_t now = millis();
    
    if (now - lastCheckTime > DEBOUNCE_TIME) {

      lastCheckTime = now;
      
      // check button 1
      byte buttonNow = digitalRead(PORT1_DIO);
    
      if (buttonNow != button1State) {
        #if DEBUG
          Serial.print("button 1 now ");
          Serial.println(buttonNow == HIGH ? "HIGH" : "LOW");
        #endif

        button1State = buttonNow;
    
        if (button1State == LOW) {
          
          #if DEBUG
            Serial.println("button 1 pressed");
          #endif
          
          // change mode
          configMode++;
          if (configMode == NUM_MODES) configMode = 0;
          
          if (saveState != UNSAVED) saveState = UNSAVED;
          displayCurrentSetting();
        }
      }
      
      // check button 2
      buttonNow = digitalRead(PORT1_AIO_AS_DIO);
    
      if (buttonNow != button2State) {
        #if DEBUG
          Serial.print("button 2 now ");
          Serial.println(buttonNow == HIGH ? "HIGH" : "LOW");
        #endif

        button2State = buttonNow;

        if (button2State == LOW) {      
          #if DEBUG
            Serial.println("button 2 pressed");
          #endif
          
          // change current setting
          changeCurrentSetting();
          displayCurrentSetting();
        }
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////
void setup() {

  // wait for things to stablise
  Sleepy::loseSomeTime(1000);

#if defined(__AVR_ATtiny84__)
  cli();
  CLKPR = bit(CLKPCE);
  CLKPR = 0; // div 1, i.e. speed up to 8 MHz
  sei();
#endif

  Serial.begin(BAUD_RATE);

  readEeprom();
  displaySettings();  

#if CONFIG_CONSOLE

  // turn on pull-up resistor for RX pin (port 2 DIO)
  pinMode(PORT2_DIO, INPUT);
  digitalWrite(PORT2_DIO, HIGH);

  // if RX is grounded, enter config
  if (digitalRead(PORT2_DIO) == LOW) {
    configConsole();
  }
  else {
    // turn off pull-up on data line
    digitalWrite(PORT2_DIO, LOW);
  }
#endif

  #if DS18B_PORT

    ds18bNumDevices = ds18b.init();

    #if DEBUG
      Serial.print("#DS18: ");
      Serial.print(ds18bNumDevices, DEC);
      Serial.println();
    #endif
  #endif

  #if DHT_PORT    

    dht.init();

    #if DEBUG
      Serial.print("DHT: ");
      Serial.print(dht.getType(), DEC);
      Serial.println();
    #endif
  #endif
  
  serialFlush();

  #if !DEBUG
    Serial.end();
  #endif
  
#if defined(__AVR_ATtiny84__)
  // power up the radio on JeenodeMicro v3
  bitSet(DDRB, 0);
  bitClear(PORTB, 0);
#endif

  // initialise transmitter
  myNodeID = rf12_initialize(myNodeID, RF12_868MHZ, myGroupID);
  rf12_control(0xC040); // set low-battery level to 2.2V instead of 3.1V

  // power down
  rf12_sleep(RF12_SLEEP);
}


/////////////////////////////////////////////////////////////////////
void loop() {
  doMeasure();
  doReport();
  
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

