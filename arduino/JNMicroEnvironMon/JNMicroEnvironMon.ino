#include <JeeLib.h>
#include <avr/sleep.h>

//#define DEBUG true

// need to include OneWire here as it won't get picked up from inside HeatHackSensors.h
#define ONEWIRE_CRC8_TABLE 0
#include <OneWire.h>

#define DHT_USE_INTERRUPTS false
#define DHT_USE_I_PIN_FOR_DATA true
#include <HeatHackSensors.h>

#include <HeatHack.h>


#define GROUP_ID 212
#define NODE_ID 2

/**
 * Default setup is:
 *
 * - DHT11/22 temp/humidty sensor on port 1 with its data line connected to the D pin.
 *
 * Intention is to have both DHT and DS18 on port 1 with DHT's data on I and DS's on D
 * and the A pin providing switchable power for both.
 */

// dht11/22 temp/humidity (uses I pin on port 1)
#define DHT_PORT 1

// Dallas DS18B (uses D pin on port 1)
#define DS18B_PORT 1


// To avoid wasting power sending frequent readings when the receiver isn't contactable,
// the node will switch to a less-frequent mode when it doesn't get acknowledgments from
// the receiver. It will switch back to normal mode as soon as an ack is received.
#define SECS_BETWEEN_TRANSMITS 10          // how frequently readings are sent normally, in seconds
#define MAX_SECS_WITHOUT_ACK 600           // how long to go without an acknowledgement before switching to less-frequent mode
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

// scheduler for timing periodic tasks
enum { MEASURE, TASK_END };
static word schedbuf[TASK_END];
Scheduler scheduler (schedbuf, TASK_END);

// interrupt handler for the Sleepy watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

static unsigned long lastAckTime = millis();
uint8_t myNodeID = NODE_ID;       // node ID used for this unit


/////////////////////////////////////////////////////////////////////
// wait a few milliseconds for proper ACK to me, return true if indeed received
static byte waitForAck() {
    MilliTimer ackTimer;
    while (!ackTimer.poll(ACK_TIME)) {
        if (rf12_recvDone() && rf12_crc == 0 &&
                // see http://talk.jeelabs.net/topic/811#post-4712
                rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
            return 1;
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_mode();
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////
//#if DEBUG
inline void serialFlush () {
    #if ARDUINO >= 100
        Serial.flush();
    #endif  
    delay(10); // make sure tx buf is empty before going back to sleep
}
//#endif

/////////////////////////////////////////////////////////////////////
// periodic report, i.e. send out a packet and optionally report on serial port
static void doReport() {
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
    
    for (byte i = 0; i < RETRY_LIMIT; i++) {
      
      if (i != 0) {
        // delay before any retry
        Sleepy::loseSomeTime(RETRY_PERIOD);
      }
      
      rf12_sleep(RF12_WAKEUP);
      rf12_sendNow(RF12_HDR_ACK, &dataPacket, dataPacket.getTransmitSize());
      rf12_sendWait(RADIO_SYNC_MODE);
      byte acked = waitForAck();
      rf12_sleep(RF12_SLEEP);

      if (acked) {
        #ifdef DEBUG  
            Serial.print(" ack ");
            Serial.println((int) i);
            serialFlush();
        #endif
        
        lastAckTime = millis();
        break;        
      }
      else {
        #ifdef DEBUG  
            Serial.print(" no ack ");
            Serial.println((int) i);
            serialFlush();
        #endif
      }
    }
}


/////////////////////////////////////////////////////////////////////
void doMeasure() {
  static bool firstMeasure = true;
  int humi, temp;
  
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

#define BUFLEN 10
char buffer[BUFLEN];

inline bool readline()
{
  static uint8_t pos = 0;

  int readch = Serial.read();

  if (readch > 0) {
    switch (readch) {
      case '\n': // Ignore new-lines
        break;
      case '\r': // Return on CR
        pos = 0;  // Reset position index ready for next time
        return true;
      default:
        if (pos < BUFLEN-1) {
          buffer[pos++] = readch;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found.
  return false;
}

byte parseInt(char* buf) {
  byte result = 0;
  char *cur = buf + 1;
  
  // find end of string
  while (*cur) cur++;
  cur--;
  
  // parse from end to start
  while (cur != buf) {
    char c = *cur;
    if (c >= '0' && c <= '9') result += (c - '0');
    cur--;
  }
  
  return result;
}

/////////////////////////////////////////////////////////////////////
void setup() {

  // wait for things to stablise
  Sleepy::loseSomeTime(1000);
  
//#ifdef DEBUG
  Serial.begin(BAUD_RATE);
  
  //Serial.println("JeeNode HeatHack Environment Monitor");

  Serial.print("g");
  Serial.print(GROUP_ID);
  Serial.print(" n");
  Serial.println(myNodeID, DEC);
  serialFlush();

  buffer[0] = 0;
  
  do {
    if (readline()) {
      switch(buffer[0]) {
        case 'n':
          // set node id: n2 to n30
          myNodeID = parseInt(buffer);
          break;
      }
      
      break;
    }
  }
  while (buffer[0] != 0 || millis() < 5000);


  Serial.print(" n");
  Serial.println(myNodeID, DEC);
  serialFlush();


//#endif

  cli();
  CLKPR = bit(CLKPCE);
  CLKPR = 0; // div 1, i.e. speed up to 8 MHz
  sei();

  // power up the radio on JeenodeMicro v3
  bitSet(DDRB, 0);
  bitClear(PORTB, 0);

  // initialise transmitter
  myNodeID = rf12_initialize(myNodeID, RF12_868MHZ, GROUP_ID);
  rf12_control(0xC040); // set low-battery level to 2.2V instead of 3.1V

  // power down
  rf12_sleep(RF12_SLEEP);

  #if DS18B_PORT
    ds18bNumDevices = ds18b.init();
  
    #if DEBUG
      Serial.print("DS18B count: ");
      Serial.println(ds18bNumDevices, DEC);
    #endif
  #endif

  #if DHT_PORT    
    #if DEBUG
      Serial.print("DHT type: ");
      Serial.println(dht.init(), DEC);
    #else
      dht.init();
    #endif
  #endif

  // do first measure immediately
  scheduler.timer(MEASURE, 0); 
}


/////////////////////////////////////////////////////////////////////
void loop() {
  
    switch (scheduler.pollWaiting(false)) {

        case MEASURE:
            doMeasure();
            doReport();

            // reschedule these measurements periodically
            unsigned long secsSinceAck = (millis() - lastAckTime)/1000;
            
            if (secsSinceAck > MAX_SECS_WITHOUT_ACK) {
              #ifdef DEBUG
                Serial.print("No ack for ");
                Serial.print(secsSinceAck);
                Serial.print(" secs. Waiting ");
                Serial.print(SECS_BETWEEN_TRANSMITS_NO_ACK);
                Serial.println(" secs till next transmit.");
                serialFlush();
              #endif

              scheduler.timer(MEASURE, SECS_BETWEEN_TRANSMITS_NO_ACK * 10);
            }
            else {
              #ifdef DEBUG
                Serial.print(SECS_BETWEEN_TRANSMITS);
                Serial.println(" secs till next transmit.");
                serialFlush();
              #endif

              scheduler.timer(MEASURE, SECS_BETWEEN_TRANSMITS * 10);
            }
    
            break;
    }
}  

