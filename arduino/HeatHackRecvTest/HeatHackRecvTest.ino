#include <JeeLib.h>
#include <HeatHack.h>

#define GROUP_ID 212
#define NODE_ID 1

// hold last seen sequence number for each node
byte lastSequence[30];

/////////////////////////////////////////////////////////////////////
void setup() {

  Serial.begin(57600);
/*  
  Serial.println("JeeNode HeatHack receive test");

  Serial.print("Using group id ");
  Serial.print(GROUP_ID);
  Serial.print(" and node id ");
  Serial.println(NODE_ID);
  Serial.println();
*/    
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
    
    byte node = rf12_hdr & RF12_HDR_MASK;
    HeatHackData *data = (HeatHackData *)rf12_data;
    
    // check sequence number. If same as last one we saw for this node
    // then data is resent so ignore it.
    if (data->sequence != lastSequence[node-1]) {
      lastSequence[node-1] = data->sequence;
 
      Serial.print("heathack ");
      Serial.print(node);
      Serial.print(" ");
        
      for (byte i=0; i<data->numReadings; i++) {
        Serial.print(data->readings[i].sensorNumber);
        Serial.print(" ");
        Serial.print(data->readings[i].sensorType);
        Serial.print(" ");
        Serial.print(data->readings[i].reading / 10);
        Serial.print(".");
        Serial.print(data->readings[i].reading % 10);
        Serial.print(" ");
      }

      Serial.println();
 
/*    
      Serial.print("\nData from node ");
      Serial.print(node);
      Serial.print(" seq ");
      Serial.println(data->sequence);
        
      for (byte i=0; i<data->numReadings; i++) {
        Serial.print("* sensor ");
        Serial.print(data->readings[i].sensorNumber);
        Serial.print(": ");
        Serial.print(HHSensorTypeNames[data->readings[i].sensorType]);
        Serial.print(" ");
        Serial.print(data->readings[i].reading / 10);
        Serial.print(".");
        Serial.println(data->readings[i].reading % 10);
      }
*/      
    }
    
    // send ack if required
    if(RF12_WANTS_ACK){
      rf12_sendStart(RF12_ACK_REPLY,0,0);
      rf12_sendWait(1);
    }    
  }
}
