/// @dir roomNode
/// New version of the Room Node (derived from rooms.pde).
// 2010-10-19 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// see http://jeelabs.org/2010/10/20/new-roomnode-code/
// and http://jeelabs.org/2010/10/21/reporting-motion/

// The complexity in the code below comes from the fact that newly detected PIR
// motion needs to be reported as soon as possible, but only once, while all the
// other sensor values are being collected and averaged in a more regular cycle.

#include <JeeLib.h>
#include <PortsSHT11.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HeatHack.h>

#define SERIAL  0   // set to 1 to also report readings on the serial port
#define DEBUG   0   // set to 1 to display each loop() run and PIR trigger

// #define SHT11_PORT  1   // defined if SHT11 is connected to a port
#define HYT131_PORT 2   // defined if HYT131 is connected to a port
#define LDR_PORT    3   // defined if LDR is connected to a port's AIO pin
//#define PIR_PORT    3   // defined if PIR is connected to a port's DIO pin

#define DS18B_PORT PORT1_DIO


#define MEASURE_PERIOD  600 // how often to measure, in tenths of seconds
#define RETRY_PERIOD    10  // how soon to retry if ACK didn't come in
#define RETRY_LIMIT     5   // maximum number of times to retry
#define ACK_TIME        10  // number of milliseconds to wait for an ack
#define REPORT_EVERY    1   // report every N measurement cycles
#define SMOOTH          3   // smoothing factor used for running averages

// set the sync mode to 2 if the fuses are still the Arduino default
// mode 3 (full powerdown) can only be used with 258 CK startup fuses
#define RADIO_SYNC_MODE 2

// sensor numbers for heathack reporting
#define SENSOR_LOWBATT  1
#define SENSOR_TEMP     2
#define SENSOR_HUMIDITY 3
#define SENSOR_LIGHT    4
#define SENSOR_MOTION   5
#define SENSOR_TEMP2    6

// The scheduler makes it easy to perform various tasks at various times:

enum { MEASURE, REPORT, TASK_END };

static word schedbuf[TASK_END];
Scheduler scheduler (schedbuf, TASK_END);

// Other variables used in various places in the code:

static byte reportCount;    // count up until next report, i.e. packet send
static byte myNodeID;       // node ID used for this unit

HeatHackData payload;

// Conditional code, depending on which sensors are connected and how:

#if SHT11_PORT
    SHT11 sht11 (SHT11_PORT);
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

#if DS18B_PORT

#define DS18B_PRECISION 9
#define REQUIRESALARMS false

byte ds18bNumDevices;
DeviceAddress ds18bAddresses[3];
bool ds18bDeviceAvailable[3];

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(DS18B_PORT);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature ds18bSensors(&oneWire);

#endif

// has to be defined because we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

// utility code to perform simple smoothing as a running average
static int smoothedAverage(int prev, int next, byte firstTime =0) {
    if (firstTime)
        return next;
    return ((SMOOTH - 1) * prev + next + SMOOTH / 2) / SMOOTH;
}

// spend a little time in power down mode while the SHT11 does a measurement
static void shtDelay () {
    Sleepy::loseSomeTime(32); // must wait at least 20 ms
}

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

// readout all the sensors and other values
static void doMeasure() {
    
    payload.clear();
  
    if (rf12_lowbat()) {
      payload.addReading(SENSOR_LOWBATT, HHSensorType::LOW_BATT, 10);
    }

    int humi, temp;
    hyt131.reading(temp, humi);

    payload.addReading(SENSOR_TEMP, HHSensorType::TEMPERATURE, temp);
    payload.addReading(SENSOR_HUMIDITY, HHSensorType::HUMIDITY, humi);
    
    ldr.digiWrite2(1);  // enable AIO pull-up
    byte light = ~ ldr.anaRead() >> 2;
    ldr.digiWrite2(0);  // disable pull-up to reduce current draw
    
    payload.addReading(SENSOR_LIGHT, HHSensorType::LIGHT, light*10);

#if PIR_PORT
    if (pir.triggered()) {
      payload.addReading(SENSOR_MOTION, HHSensorType::MOTION, 10);
    }
#endif

#if DS18B_PORT

    if (ds18bNumDevices > 0) {

      ds18bSensors.requestTemperatures();
      Sleepy::loseSomeTime(ds18bSensors.millisToWaitForConversion(DS18B_PRECISION));

      for (int i=0; i<3; i++) {
        if (ds18bDeviceAvailable[i]) {    
          int tempC = ds18bSensors.getTempC(ds18bAddresses[i]) * 10;
          payload.addReading(SENSOR_TEMP2 + i, HHSensorType::TEMPERATURE, tempC);
        }
      }      
    }
#endif
}

static void serialFlush () {
    #if ARDUINO >= 100
        Serial.flush();
    #endif  
    delay(2); // make sure tx buf is empty before going back to sleep
}

// periodic report, i.e. send out a packet and optionally report on serial port
static void doReport() {
    rf12_sleep(RF12_WAKEUP);
    rf12_sendNow(0, &payload, payload.getTransmitSize());
    rf12_sendWait(RADIO_SYNC_MODE);
    rf12_sleep(RF12_SLEEP);

    #if SERIAL
        Serial.println("--------------------------");

        for (byte i=0; i<payload.numReadings; i++) {
          Serial.print("* sensor ");
          Serial.print(payload.readings[i].sensorNumber);
          Serial.print(": ");
          Serial.print(HHSensorTypeNames[payload.readings[i].sensorType]);
          Serial.print(" ");
          Serial.println((float)(payload.readings[i].reading / 10.0));
        }
        serialFlush();
    #endif
}

// send packet and wait for ack when there is a motion trigger
static void doTrigger() {

    for (byte i = 0; i < RETRY_LIMIT; ++i) {
        rf12_sleep(RF12_WAKEUP);
        rf12_sendNow(RF12_HDR_ACK, &payload, sizeof payload);
        rf12_sendWait(RADIO_SYNC_MODE);
        byte acked = waitForAck();
        rf12_sleep(RF12_SLEEP);

        if (acked) {
            #if DEBUG
                Serial.print(" ack ");
                Serial.println((int) i);
                serialFlush();
            #endif
            // reset scheduling to start a fresh measurement cycle
            scheduler.timer(MEASURE, MEASURE_PERIOD);
            return;
        }
        
        delay(RETRY_PERIOD * 100);
    }
    scheduler.timer(MEASURE, MEASURE_PERIOD);
    #if DEBUG
        Serial.println(" no ack!");
        serialFlush();
    #endif
}

void blink (byte pin) {
    for (byte i = 0; i < 6; ++i) {
        delay(100);
        digitalWrite(pin, !digitalRead(pin));
    }
}

void setup () {
    #if SERIAL || DEBUG
        Serial.begin(57600);
        Serial.print("\n[roomNode.3]");
        myNodeID = rf12_config();
        serialFlush();
    #else
        myNodeID = rf12_config(0); // don't report info on the serial port
    #endif
    
    rf12_sleep(RF12_SLEEP); // power down
    
    #if PIR_PORT
        pir.digiWrite(PIR_PULLUP);
        #ifdef PCMSK2
          bitSet(PCMSK2, PIR_PORT + 3);
          bitSet(PCICR, PCIE2);
        #else
          //XXX TINY!
        #endif
    #endif

    #if DS18B_PORT
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
    #endif

    reportCount = REPORT_EVERY;     // report right away for easy debugging
    scheduler.timer(MEASURE, 0);    // start the measurement loop going
}

void loop () {
    switch (scheduler.pollWaiting()) {

        case MEASURE:
            // reschedule these measurements periodically
            scheduler.timer(MEASURE, MEASURE_PERIOD);
    
            doMeasure();

            // every so often, a report needs to be sent out
            if (++reportCount >= REPORT_EVERY) {
                reportCount = 0;
                scheduler.timer(REPORT, 0);
            }
            break;
            
        case REPORT:
            doReport();
            break;
    }
}
