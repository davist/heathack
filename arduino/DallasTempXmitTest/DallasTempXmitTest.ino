#include <OneWire.h>
#include <DallasTemperature.h>
#include <JeeLib.h>
#include <HeatHack.h>

//#define DEBUG 1

#define GROUP_ID 212
#define NODE_ID 2

#define MS_BETWEEN_TRANSMITS 5000
#define MS_BETWEEN_TRANSMITS_NO_ACK 60000
#define MAX_TIME_WITHOUT_ACK 60000

#define TEMP_PIN PORT1_DIO
#define TEMPERATURE_PRECISION 9
#define REQUIRESALARMS false

DeviceAddress deviceAddress;
bool isDeviceAvailable = false;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(TEMP_PIN);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

HeatHackData dataPacket;

#define TEMP_SENSOR_NUM 1

// interrupt handler for the Sleepy watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

/////////////////////////////////////////////////////////////////////
void setup() {

#ifdef DEBUG  
  Serial.begin(9600);
  
  Serial.println("JeeNode Dallas Temperature transmit test");

  Serial.print("Using group id ");
  Serial.print(GROUP_ID);
  Serial.print(" and node id ");
  Serial.println(NODE_ID);
#endif

  // initialise transmitter
  rf12_initialize(NODE_ID, RF12_868MHZ, GROUP_ID);
  rf12_easyInit(0);
  
  // initialise temp sensor
  sensors.begin();
  sensors.setWaitForConversion(false);
  
  // Grab a count of devices on the wire
  int numberOfDevices = sensors.getDeviceCount();

  if (sensors.getDeviceCount() > 0) {
    if (sensors.getAddress(deviceAddress, 0)) {
      isDeviceAvailable = true;
      sensors.setResolution(deviceAddress, TEMPERATURE_PRECISION);
    }
    else {
      isDeviceAvailable = false;      
    }
  }

#ifdef DEBUG  
  if (isDeviceAvailable) {
    Serial.print("Sensor address is ");
    for (byte i = 0; i < 8; i++)
    {
      if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
    }
    Serial.println();
  }
  else {
    Serial.println("No sensors found");
  }
  Serial.println();
#endif

}


/////////////////////////////////////////////////////////////////////
void loop() {
  if (!isDeviceAvailable) return;
  
  static unsigned long lastAckTime = millis();
  
  // get reading
  sensors.requestTemperatures();

  // wait for sensor to acquire reading
#ifdef DEBUG
  delay(sensors.millisToWaitForConversion(TEMPERATURE_PRECISION));
#else
  Sleepy::loseSomeTime(sensors.millisToWaitForConversion(TEMPERATURE_PRECISION));
#endif

  int tempC = sensors.getTempC(deviceAddress) * 10;

#ifdef DEBUG   
  Serial.print("Got reading: ");
  Serial.println(tempC/10.0);
#endif

  // format data packet
  dataPacket.clear();
  dataPacket.addReading(TEMP_SENSOR_NUM, TEMPERATURE, tempC);
  
  // transmit it
  bool awaitingAck = rf12_easySend((const void *)(&dataPacket), dataPacket.getTransmitSize());
  
#ifdef DEBUG  
  if (!awaitingAck) {
    Serial.println("Data unchanged. Not sending");
  }
#endif

  if (awaitingAck) {
    char pollResult;
    bool ackReceived;

    // easyPoll does the actual sending
    rf12_sleep(RF12_WAKEUP);
   
    do {
      pollResult = rf12_easyPoll();
      
      switch (pollResult) {
        
        case -1:
          // sending data or waiting for ack
          break;
          
        case 0:
          // empty ack received or timed out

          if (rf12_hdr == (RF12_HDR_CTL | RF12_HDR_DST | NODE_ID)) {
            // ack received
            ackReceived = true;
          }
          else {
            ackReceived = false;
          }
          
          awaitingAck = false;
          break;        
        
        case 1:
          // ack with data received
          awaitingAck = false;
          ackReceived = true;
          break;
      }    
    } while (pollResult == -1);
    
    if (ackReceived) {
      lastAckTime = millis();
#ifdef DEBUG
      Serial.println("ACK received");
    }
    else {
      Serial.println("No ACK. Timed out");
#endif
    }
    
  }
  
  // power down transmitter until next transmission
  rf12_sleep(RF12_SLEEP);
  
  unsigned long waitTime;
  if (millis() - lastAckTime > MAX_TIME_WITHOUT_ACK) {
    waitTime = MS_BETWEEN_TRANSMITS_NO_ACK;
  }
  else {
    waitTime = MS_BETWEEN_TRANSMITS;
  }
  
#ifdef DEBUG  
  // busy delay to avoid breaking serial port
  delay(waitTime);
#else
  // low power delay
  Sleepy::loseSomeTime(waitTime);
#endif
}
