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
 * attaching the Configurator and serial terminal. If the Configurator isn't available, 2 push buttons
 * on a breadboard can be used, or even just 2 wires touched to ground!
 *
 * Remove any sensor plug that's attached to the Micro and fit the Configurator. Attach the
 * cable to an FTDI serial port (eg USB BUB).
 * Alternatively, connect wires to the DIO, AIO, RX and GND pins. Touch AIO to GND for one button,
 * and DIO to GND for the other.
 * To enter config mode at startup, connect RX to GND.
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
 * Remove the Configurator and reattach the sensor.
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

// Override default node id
#define DEFAULT_NODE_ID 20

// override default min/max to match what the Micro's config console supports
#define GROUP_MIN 200
#define GROUP_MAX 212

#define NODE_MIN 20
#define NODE_MAX 30

#define INTERVAL_MIN 1  // 10 secs min
#define INTERVAL_MAX 60 // 10 mins max

#include <JeeLib.h>

// need to include OneWire here as it won't get picked up from inside HeatHackSensors.h
#define ONEWIRE_CRC8_TABLE 0
#include <OneWire.h>

#define DHT_USE_INTERRUPTS false
#define DHT_USE_I_PIN_FOR_DATA true
#include "HeatHackSensors.h"

#include "HeatHack.h"
#include "HeatHackShared.h"


#if DHT_PORT
  DHT dht(DHT_PORT);
#endif

#if DS18B_PORT
  DS18B ds18b(DS18B_PORT);
  uint8_t ds18bNumDevices;
#endif

#define SENSOR_LOWBATT  1
#define SENSOR_TEMP     2
#define SENSOR_HUMIDITY 3
#define SENSOR_TEMP2    4  // base for DS18 sensors

enum ConfigMode { GROUP, NODE, INTERVAL, SAVE, NUM_MODES };
enum SaveState { UNSAVED, SAVED };

uint8_t saveState = UNSAVED;
uint8_t configMode = GROUP;


/////////////////////////////////////////////////////////////////////
inline void doMeasure(void) {
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
        button1State = buttonNow;
    
        if (button1State == LOW) {
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
        button2State = buttonNow;

        if (button2State == LOW) {      
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

  readEeprom();

  Serial.begin(BAUD_RATE);
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
  rf12_initialize(myNodeID, RF12_868MHZ, myGroupID);
  rf12_control(0xC040); // set low-battery level to 2.2V instead of 3.1V

  // power down
  rf12_sleep(RF12_SLEEP);
}


/////////////////////////////////////////////////////////////////////
void loop() {
  doMeasure();
  doReport();
  doSleep();
}  

