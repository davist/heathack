// enable code in HeatHack lib that's just for decoding received packets
#define RECEIVER_NODE true

#include <JeeLib.h>
#include <OneWire.h>
#include <HeatHack.h>
#include <HeatHackShared.h>

// holds last seen sequence number for each node
byte lastSequence[30];

/////////////////////////////////////////////////////////////////////
void setup() {

  readEeprom();

  Serial.begin(BAUD_RATE);
  
  Serial.print("JeeNode HeatHack Receiver v");
  Serial.println(VERSION);

  configConsole();

  Serial.println();  
  Serial.print(F("Using group id "));
  Serial.print(myGroupID);
  Serial.print(F(" and node id "));
  Serial.println(RECEIVER_NODE_ID);
  
  if (eepromFlags & FLAG_ACK) {
    Serial.println(F("Acknowledgements enabled (this is the main receiver)"));
  }
  else {
    Serial.println(F("Acknowledgements disabled (this is a secondary receiver - listening only)"));
  }
  Serial.println();  
  
  // initialise transceiver
  rf12_initialize(RECEIVER_NODE_ID, RF12_868MHZ, myGroupID);
  
  // init lastSequences to unused values
  for (byte i=0; i<30; i++) {
    lastSequence[i] = 0xff;
  }
}


/////////////////////////////////////////////////////////////////////
void loop() {
  if (rf12_recvDone() && rf12_crc == 0) {
    // valid data received
    
    // get node id from packet header
    byte node = rf12_hdr & RF12_HDR_MASK;

    bool sentAck = false;

    if (eepromFlags & FLAG_ACK) {
      // send ack immediately to avoid delays caused by time taken to write to serial port
      if(RF12_WANTS_ACK){
        rf12_sendStart(RF12_ACK_REPLY, 0, 0);
        rf12_sendWait(1);
        
        sentAck = true;
      }
    }
    
    // packet data is a HeatHackData struct
    HeatHackData *data = (HeatHackData *)rf12_data;
    
    // check sequence number. If same as last one we saw for this node
    // then data is resent so ignore it.
    bool isRepeat = false;
    
    if (data->sequence != lastSequence[node-1]) {
      lastSequence[node-1] = data->sequence;

      // print out sensor readings on the serial port
      // format is: heathack <node id> <port num><sensor num> <sensor type> <reading> <port num><sensor num> <sensor type> <reading> ...(repeated for each reading)
      // Note port and sensor numbers are combined to report a single two-digit sensor number
      
      // start each line with a known string so that the code reading from the serial port can ignore spurious data
      Serial.print("heathack ");
      Serial.print(node);
      Serial.print(" ");
        
      for (byte i=0; i<data->numReadings; i++) {
        uint8_t sensorType = data->readings[i].sensorType;

        if (sensorType == HHSensorType::LOW_BATT) {
          // "low battery" isn't a real sensor (not attached to a port) so ignore port/sensor number and always use 1
          Serial.print("1");
        }
        else {
          Serial.print(data->readings[i].getPort());
          Serial.print(data->readings[i].getSensor());
        }
        Serial.print(" ");
        Serial.print(sensorType);
        Serial.print(" ");
        Serial.print(data->readings[i].getIntPartOfReading());
        
        uint8_t decimal = data->readings[i].getDecPartOfReading();        
        if (decimal != NO_DECIMAL) {
          // display as decimal value to 1 decimal place
          Serial.print(".");
          Serial.print(decimal);
        }        
        Serial.print(" ");
      }

      Serial.println();
      serialFlush();

      // flash LED to indicate packet received
      flashLED();
    }
    else {
      isRepeat = true;
    }

    if (eepromFlags & FLAG_VERBOSE) {
      Serial.print(F("\n\rData from node "));
      Serial.print(node);
      Serial.print(F(" seq "));
      Serial.print(data->sequence);

      if (isRepeat) {        
        Serial.print(F(" (repeated sequence id)"));
      }
      Serial.println();
      
      for (byte i=0; i<data->numReadings; i++) {
        uint8_t sensorType = data->readings[i].sensorType;

        Serial.print("* ");

        // "low battery" isn't a real sensor (not attached to a port) so ignore port/sensor number and always use 1
        if (sensorType != HHSensorType::LOW_BATT) {
          Serial.print(F("port "));
          Serial.print(data->readings[i].getPort());
          Serial.print(F(" sensor "));
          Serial.print(data->readings[i].getSensor());
          Serial.print(F(": "));
        }
        Serial.print(HHSensorTypeNames[sensorType]);
        Serial.print(" ");
        Serial.print(data->readings[i].getIntPartOfReading());
        
        uint8_t decimal = data->readings[i].getDecPartOfReading();        
        if (decimal != NO_DECIMAL) {
          // display as decimal value to 1 decimal place
          Serial.print(".");
          Serial.print(decimal);
        }
        Serial.println();
      }

      if (sentAck) {
        Serial.println(F("Sent ack"));
        Serial.println();
      }
    }
  }
}
