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
//#define DEBUG true

#define ENABLE_DHT true
#define ENABLE_DS18B true

// Enable config console. Comment out to disable.
#define CONFIG_CONSOLE true

// Override default node id
#define DEFAULT_NODE_ID 20

// override default min/max to match what the Micro's config console supports
#define GROUP_MIN 200
#define GROUP_MAX 212

#define NODE_MIN 20
#define NODE_MAX 29

#define INTERVAL_MIN 1  // 10 secs min
#define INTERVAL_MAX 60 // 10 mins max

#define DHT_USE_INTERRUPTS false

#include <JeeLib.h>

// need to include OneWire here as it won't get picked up from inside HeatHackSensors.h
#include <OneWire.h>

#include "HeatHack.h"
#include "HeatHackShared.h"
#include "HeatHackSensors.h"

// sensor classes
#if ENABLE_DHT
DHT dht(PORT1_DIO, SENSOR_NONE);
#endif

#if ENABLE_DS18B
DS18B ds18b(PORT1_DIO);
#endif

enum ConfigMode { GROUP, NODE, INTERVAL, SAVE, NUM_MODES };


/////////////////////////////////////////////////////////////////////
inline void doMeasure(void) {
  static bool firstMeasure = true;
  
  // format data packet
  dataPacket.clear();

  // send a reading first time even if battery isn't low so that the logger
  // knows about the sensor and can graph it for when it does start reporting a low batt.
  bool lowBatt = rf12_lowbat();
  if (lowBatt || firstMeasure) {
    HHReading reading;
    reading.sensorType = HHSensorType::LOW_BATT;
    reading.encodedReading = lowBatt;
    dataPacket.addReading(reading);
  }

#if ENABLE_DHT
  dht.reading(dataPacket);
#endif

#if ENABLE_DS18B
  ds18b.reading(dataPacket);
#endif

  firstMeasure = false;
}


static void printGroup(void) {
  const char* numstr = "00\001\002\003\004\005\006\007\008\009\010\011\012";
  Serial.write(" g2");
  Serial.write(numstr + ((myGroupID - 200) * 3));
}

static void printNode(void) {
  const char* numstr = "0\01\02\03\04\05\06\07\08\09";
  Serial.write(" n2");
  Serial.write(numstr + ((myNodeID - 20) << 1));
}

static void printInterval(void) {
  const char* istr = "10\060\0600";
  const char *curIstr;
  
  Serial.write(" i");
  if (myInterval == 1) curIstr = istr;
  else if (myInterval == 6) curIstr = istr + 3;
  else curIstr = istr + 6;
  Serial.write(curIstr);
}

/////////////////////////////////////////////////////////////////////
inline void displaySettings(void) {
  Serial.write("\r\n v" VERSION " ");
  printGroup();
  printNode();
  printInterval();
  Serial.write("\r\n");
}

/////////////////////////////////////////////////////////////////////
void displayCurrentSetting(uint8_t configMode) {

  // begin with carriage return to reset cursor to start of line
  Serial.write("\r");
  
  if (configMode == GROUP) {
    printGroup();
  }
  else if (configMode == NODE) {
    printNode();
  }
  else if (configMode == INTERVAL) {
    printInterval();
  }
  else {
      Serial.write("save");
  }

  // blank out any left over text from previous setting
  Serial.write("  ");
}

/////////////////////////////////////////////////////////////////////
void changeCurrentSetting(uint8_t configMode) {

  if (configMode == GROUP) {
    if (myGroupID < GROUP_MAX) myGroupID++;
    else myGroupID = GROUP_MIN;
  }
  
  else if (configMode == NODE) {
    if (myNodeID < NODE_MAX) myNodeID++;
    else myNodeID = NODE_MIN;
  }
  
  else if (configMode == INTERVAL) {
    if      (myInterval == 1) myInterval = 6;
    else if (myInterval == 6) myInterval = 60;
    else                      myInterval = 1;
  }
  
  else { // SAVE
    writeEeprom();
  }
}

/////////////////////////////////////////////////////////////////////
void configConsole(void) {

  // use A and D as button inputs with pull-up enabled
  pinMode(PORT1_DIO, INPUT);
  pinMode(PORT1_AIO_AS_DIO, INPUT);
  digitalWrite(PORT1_DIO, HIGH);
  digitalWrite(PORT1_AIO_AS_DIO, HIGH);
  
  byte button1State = LOW;
  byte button2State = LOW;
  uint32_t lastCheckTime = 0;
  const uint32_t DEBOUNCE_TIME = 100;
  uint8_t configMode = GROUP;
  
  Serial.write("Config\r\n");
  displayCurrentSetting(configMode);

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
          displayCurrentSetting(configMode);
        }
      }
      
      // check button 2
      buttonNow = digitalRead(PORT1_AIO_AS_DIO);
    
      if (buttonNow != button2State) {
        button2State = buttonNow;

        if (button2State == LOW) {      
          // change current setting
          changeCurrentSetting(configMode);
          displayCurrentSetting(configMode);
        }
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////
void setup() {

  // wait for things to stablise
//  Sleepy::loseSomeTime(1000);

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

  bool dhtOnIPin = false;

#if ENABLE_DHT && ENABLE_DS18B
  // check for a OneWire bus (DS18B20 sensors) on D pin
  OneWire oneWire;
  oneWire.init(PORT1_DIO);

  DeviceAddress ds18Addr;

  if (oneWire.search(ds18Addr) &&
    oneWire.crc8(ds18Addr, 7) == ds18Addr[7]) {

    // found DS18B on D pin
    ds18b.init();

    // setup DHT on I pin
    dhtOnIPin = true;
  }
  else {
    // assume DHT is on D pin
    dhtOnIPin = false;
  }
#elif ENABLE_DHT
  dhtOnIPin = false;
#else
  ds18b.init();

  #if DEBUG
    Serial.print("#DS18: ");
    Serial.print(ds18b.getNumDevices(), DEC);
    Serial.println();
  #endif

#endif

  #if ENABLE_DHT
    if (dhtOnIPin) dht.initOnIPin();
    else  dht.init();

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

  transmitPower = findMinTransmitPower();

  // power down
  rf12_sleep(RF12_SLEEP);
}


/////////////////////////////////////////////////////////////////////
void loop() {
  doMeasure();
  doReport();
  doSleep();
}  

