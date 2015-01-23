#include <JeeLib.h>
#include <HeatHack.h>

 #define DEBUG 1

#define GROUP_ID 212
#define NODE_ID 1

// holds last seen sequence number for each node
byte lastSequence[30];

/////////////////////////////////////////////////////////////////////
void setup() {

  Serial.begin(BAUD_RATE);
  
  Serial.println("JeeNode HeatHack Receiver");

  Serial.print("Using group id ");
  Serial.print(GROUP_ID);
  Serial.print(" and node id ");
  Serial.println(NODE_ID);
  Serial.println();
    
  // initialise transceiver
  rf12_initialize(NODE_ID, RF12_868MHZ, GROUP_ID);
  
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

    // send ack immediately to avoid delays caused by time taken to write to serial port
    if(RF12_WANTS_ACK){
      rf12_sendStart(RF12_ACK_REPLY,0,0);
      rf12_sendWait(1);
      #if DEBUG
        Serial.print("Sent ack to node ");
        Serial.println(node);
        Serial.println();
      #endif
    }
    
    // packet data is a HeatHackData struct
    HeatHackData *data = (HeatHackData *)rf12_data;
    
    // check sequence number. If same as last one we saw for this node
    // then data is resent so ignore it.
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
    }
    else {
      #if DEBUG
        Serial.print("\n\rRepeated sequence id ");
        Serial.print(data->sequence);
        Serial.print(" from node ");
        Serial.println(node);
      #endif
    }

    #if DEBUG    
      Serial.print("\n\rData from node ");
      Serial.print(node);
      Serial.print(" seq ");
      Serial.println(data->sequence);
        
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
      Serial.println();
    #endif      
  }
}
