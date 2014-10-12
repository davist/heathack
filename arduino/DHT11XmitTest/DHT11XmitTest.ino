#include <JeeLib.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <idDHT11.h>
#include <HeatHack.h>

//#define DEBUG 1

#define GROUP_ID 212
#define NODE_ID 2

#define DHT11_PIN PORT_IRQ_AS_DIO
#define DHT11_INTERRUPT PORT_IRQ

#define SECS_BETWEEN_TRANSMITS 60
#define SECS_BETWEEN_TRANSMITS_NO_ACK 600
#define MAX_SECS_WITHOUT_ACK 600

#define ACK_TIME        10  // number of milliseconds to wait for an ack
#define RETRY_PERIOD    1000  // how soon to retry if ACK didn't come in
#define RETRY_LIMIT     5   // maximum number of times to retry

// set the sync mode to 2 if the fuses are still the Arduino default
// mode 3 (full powerdown) can only be used with 258 CK startup fuses
#define RADIO_SYNC_MODE 2

static byte myNodeID;       // node ID used for this unit

void dht11_wrapper(); // must be declared before the lib initialization

idDHT11 dht11(DHT11_PIN, DHT11_INTERRUPT, dht11_wrapper);

void dht11_wrapper() {
  dht11.isrCallback();
}

HeatHackData dataPacket;

#define SENSOR_LOWBATT  1
#define SENSOR_TEMP     2
#define SENSOR_HUMIDITY 3

// scheduler for timing periodic tasks
enum { MEASURE, TASK_END };
static word schedbuf[TASK_END];
Scheduler scheduler (schedbuf, TASK_END);

// interrupt handler for the Sleepy watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

static unsigned long lastAckTime = millis();


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
static void serialFlush () {
    #if ARDUINO >= 100
        Serial.flush();
    #endif  
    delay(10); // make sure tx buf is empty before going back to sleep
}


/////////////////////////////////////////////////////////////////////
// periodic report, i.e. send out a packet and optionally report on serial port
static void doReport() {
    #ifdef DEBUG  
        Serial.println("--------------------------");

        for (byte i=0; i<dataPacket.numReadings; i++) {
          Serial.print("* sensor ");
          Serial.print(dataPacket.readings[i].sensorNumber);
          Serial.print(": ");
          Serial.print(HHSensorTypeNames[dataPacket.readings[i].sensorType]);
          Serial.print(" ");
          Serial.println((float)(dataPacket.readings[i].reading / 10.0));
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
  // format data packet
  dataPacket.clear();

  if (rf12_lowbat()) {
    dataPacket.addReading(SENSOR_LOWBATT, HHSensorType::LOW_BATT, 10);
  }

  dht11.acquire();

  while (dht11.acquiring()) {
    // wait for interrupt
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_mode();
  }

  if (dht11.getStatus() == IDDHTLIB_OK) {
    dataPacket.addReading(SENSOR_TEMP, HHSensorType::TEMPERATURE, dht11.getCelsius() * 10);
    dataPacket.addReading(SENSOR_HUMIDITY, HHSensorType::HUMIDITY, dht11.getHumidity() * 10);
  }

}


/////////////////////////////////////////////////////////////////////
void setup() {

  // wait for things to stablise
  delay(100);
  
#ifdef DEBUG  
  Serial.begin(57600);
  
  Serial.println("JeeNode DHT11 temp & humidity transmit test");

  Serial.print("Using group id ");
  Serial.print(GROUP_ID);
  Serial.print(" and node id ");
  Serial.println(NODE_ID);
  serialFlush();
#endif

  // initialise transmitter
  myNodeID = rf12_initialize(NODE_ID, RF12_868MHZ, GROUP_ID);
  // power down
  rf12_sleep(RF12_SLEEP);

  // do first measure immediately
  scheduler.timer(MEASURE, 0); 
}


/////////////////////////////////////////////////////////////////////
void loop() {
  
    switch (scheduler.pollWaiting()) {

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

