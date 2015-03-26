// enable code in HeatHack lib that's just for decoding received packets
#define RECEIVER_NODE true

#include <JeeLib.h>
#include <HeatHack.h>
#include <HeatHackShared.h>

// holds last seen sequence number for each node
byte lastSequence[30];

/////////////////////////////////////////////////////////////////////
void setup() {

  readEeprom();

  Serial.begin(BAUD_RATE);
  
  Serial.println("JeeNode HeatHack Receiver");

  configConsole();

  Serial.println();  
  Serial.print("Using group id ");
  Serial.print(myGroupID);
  Serial.print(" and node id ");
  Serial.println(RECEIVER_NODE_ID);
  
  if (receiverFlags & RX_FLAG_ACK) {
    Serial.println("Acknowledgements enabled (this is the main receiver)");
  }
  else {
    Serial.println("Acknowledgements disabled (this is a secondary receiver - listening only)");
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

    if (receiverFlags & RX_FLAG_ACK) {
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
      // format is: heathack <node id> <sensor num> <sensor type> <reading> <sensor num> <sensor type> <reading> ...(repeated for each reading)
      
      // start each line with a known string so that the code reading from the serial port can ignore spurious data
      Serial.print("heathack ");
      Serial.print(node);
      Serial.print(" ");
        
      for (byte i=0; i<data->numReadings; i++) {
        Serial.print(data->readings[i].sensorNumber);
        Serial.print(" ");
        Serial.print(data->readings[i].sensorType);
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

    if (receiverFlags & RX_FLAG_VERBOSE) {
      Serial.print("\n\rData from node ");
      Serial.print(node);
      Serial.print(" seq ");
      Serial.print(data->sequence);

      if (isRepeat) {        
        Serial.print(" (repeated sequence id)");
      }
      Serial.println();
      
      for (byte i=0; i<data->numReadings; i++) {
        Serial.print("* sensor ");
        Serial.print(data->readings[i].sensorNumber);
        Serial.print(": ");
        Serial.print(HHSensorTypeNames[data->readings[i].sensorType]);
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
        Serial.println("Sent ack");
        Serial.println();
      }
    }
  }
}
