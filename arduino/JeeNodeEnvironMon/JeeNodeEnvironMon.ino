#include <JeeLib.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <idDHT11.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HeatHack.h>

//#define DEBUG 1

#define GROUP_ID 212
#define NODE_ID 4

// dht11 temp/humidity
#define DHT11_PIN PORT_IRQ_AS_DIO
#define DHT11_INTERRUPT PORT_IRQ

// Dallas DS18B switchable parasitic power board
#if defined(__AVR_ATtiny84__) // JNmicro
  #define DS18B_DATA_PIN PORT1_DIO
  #define DS18B_POWER_PIN PORT1_AIO_AS_DIO
#else
  #define DS18B_DATA_PIN PORT4_DIO
  #define DS18B_POWER_PIN PORT4_AIO_AS_DIO
#endif

// room node module
#define HYT131_PORT 2   // defined if HYT131 is connected to a port
#define LDR_PORT    3   // defined if LDR is connected to a port's AIO pin
//#define PIR_PORT    3   // defined if PIR is connected to a port's DIO pin

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

#if DS18B_DATA_PIN

  #define DS18B_PRECISION 9
  #define REQUIRESALARMS false
  
  byte ds18bNumDevices;
  DeviceAddress ds18bAddresses[3];
  bool ds18bDeviceAvailable[3];
  
  // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
  OneWire oneWire(DS18B_DATA_PIN);
  
  // Pass our oneWire reference to Dallas Temperature. 
  DallasTemperature ds18bSensors(&oneWire);

  void enableDS18B() {
    digitalWrite(DS18B_POWER_PIN, HIGH);
    delayMicroseconds(50);
  }

  void disableDS18B() {
    digitalWrite(DS18B_POWER_PIN, LOW);
  }


#endif

#if HYT131_PORT
    PortI2C hyti2cport (HYT131_PORT);
    HYT131 hyt131 (hyti2cport);
#endif

#if LDR_PORT
    Port ldr (LDR_PORT);
#endif

#if PIR_PORT
    #define PIR_HOLD_TIME   30  // hold PIR value this many seconds after change
    #define PIR_PULLUP      1   // set to one to pull-up the PIR input pin
    #define PIR_INVERTED    1   // 0 or 1, to match PIR reporting high or low
    
    /// Interface to a Passive Infrared motion sensor.
    class PIR : public Port {
        volatile byte value, changed;
        volatile uint32_t lastOn;
    public:
        PIR (byte portnum)
            : Port (portnum), value (0), changed (0), lastOn (0) {}

        // this code is called from the pin-change interrupt handler
        void poll() {
            // see http://talk.jeelabs.net/topic/811#post-4734 for PIR_INVERTED
            byte pin = digiRead() ^ PIR_INVERTED;
            // if the pin just went on, then set the changed flag to report it
            if (pin) {
                if (!state())
                    changed = 1;
                lastOn = millis();
            }
            value = pin;
        }

        // state is true if curr value is still on or if it was on recently
        byte state() const {
            byte f = value;
            if (lastOn > 0)
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                    if (millis() - lastOn < 1000 * PIR_HOLD_TIME)
                        f = 1;
                }
            return f;
        }

        // return true if there is new motion to report
        byte triggered() {
            byte f = changed;
            changed = 0;
            return f;
        }
    };

    PIR pir (PIR_PORT);

    // the PIR signal comes in via a pin-change interrupt
    ISR(PCINT2_vect) { pir.poll(); }
#endif


HeatHackData dataPacket;

#define SENSOR_LOWBATT  1
#define SENSOR_TEMP     2
#define SENSOR_HUMIDITY 3
#define SENSOR_LIGHT    4
#define SENSOR_MOTION   5
#define SENSOR_TEMP2    6

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
  else {
    dataPacket.addReading(SENSOR_LOWBATT, HHSensorType::LOW_BATT, 0);
  }

#if DHT11_PIN
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
#endif

#if DS18B_DATA_PIN

  if (ds18bNumDevices > 0) {

    enableDS18B();
    ds18bSensors.requestTemperatures();
    Sleepy::loseSomeTime(ds18bSensors.millisToWaitForConversion(DS18B_PRECISION));

    for (int i=0; i<3; i++) {
      if (ds18bDeviceAvailable[i]) {    
        int tempC = ds18bSensors.getTempC(ds18bAddresses[i]) * 10;
        dataPacket.addReading(SENSOR_TEMP2 + i, HHSensorType::TEMPERATURE, tempC);
      }
    }
    disableDS18B();
  }
#endif

#if HYT131_PORT
    int humi, temp;
    hyt131.reading(temp, humi);

    payload.addReading(SENSOR_TEMP, HHSensorType::TEMPERATURE, temp);
    payload.addReading(SENSOR_HUMIDITY, HHSensorType::HUMIDITY, humi);
#endif

#if LDR_PORT
    ldr.digiWrite2(1);  // enable AIO pull-up
    byte light = ~ ldr.anaRead() >> 2;
    ldr.digiWrite2(0);  // disable pull-up to reduce current draw
    
    payload.addReading(SENSOR_LIGHT, HHSensorType::LIGHT, light*10);
#endif

#if PIR_PORT
    if (pir.triggered()) {
      payload.addReading(SENSOR_MOTION, HHSensorType::MOTION, 10);
    }
#endif

}


/////////////////////////////////////////////////////////////////////
void setup() {

  // wait for things to stablise
  delay(1000);
  
#ifdef DEBUG  
  Serial.begin(57600);
  
  Serial.println("JeeNode DHT11 temp & humidity transmit test");

  Serial.print("Using group id ");
  Serial.print(GROUP_ID);
  Serial.print(" and node id ");
  Serial.println(NODE_ID);
  serialFlush();
#endif

  cli();
  CLKPR = bit(CLKPCE);
#if defined(__AVR_ATtiny84__) // JNmicro
  CLKPR = 0; // div 1, i.e. speed up to 8 MHz
#else
  CLKPR = 1; // div 2, i.e. slow down to 8 MHz
#endif
  sei();

#if defined(__AVR_ATtiny84__)
  // power up the radio on JeenodeMicro v3
  bitSet(DDRB, 0);
  bitClear(PORTB, 0);
#endif

  #if PIR_PORT
    pir.digiWrite(PIR_PULLUP);
    #ifdef PCMSK2
      bitSet(PCMSK2, PIR_PORT + 3);
      bitSet(PCICR, PCIE2);
    #else
      //XXX TINY!
    #endif
  #endif

  #if DS18B_DATA_PIN
  pinMode(DS18B_POWER_PIN, OUTPUT);

  enableDS18B();

  ds18bSensors.begin();
  ds18bSensors.setWaitForConversion(false);
  
  // Grab a count of devices on the wire
  ds18bNumDevices = ds18bSensors.getDeviceCount();

  #if DEBUG
    Serial.print("DS18B count: ");
    Serial.println(ds18bNumDevices);
  #endif

  // can only handle 3 max
  if (ds18bNumDevices > 3) ds18bNumDevices = 3;

  if (ds18bNumDevices > 0) {
    for (byte i=0; i<ds18bNumDevices; i++) {
      if (ds18bSensors.getAddress(ds18bAddresses[i], i)) {
        ds18bDeviceAvailable[i] = true;
        ds18bSensors.setResolution(ds18bAddresses[i], DS18B_PRECISION);

        #if DEBUG
          Serial.print(" device ");
          Serial.print(i);
          Serial.print(", address ");
          Serial.print(ds18bAddresses[i][0], HEX);
          Serial.print(ds18bAddresses[i][1], HEX);
          Serial.print(ds18bAddresses[i][2], HEX);
          Serial.println(ds18bAddresses[i][3], HEX);
        #endif
      }
      else {
        ds18bDeviceAvailable[i] = false;
      }
    }
  }

  disableDS18B();
  
  #endif

  // initialise transmitter
  myNodeID = rf12_initialize(NODE_ID, RF12_868MHZ, GROUP_ID);
  rf12_control(0xC040); // set low-battery level to 2.2V instead of 3.1V

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

