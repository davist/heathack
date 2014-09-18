#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 3
#define TEMPERATURE_PRECISION 9
#define REQUIRESALARMS false

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

DeviceAddress deviceAddress;
bool isDeviceAvailable;

void setup(void)
{
  // start serial port
  Serial.begin(9600);

  // Start up the library
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
  
}


void loop(void)
{ 
  if (!isDeviceAvailable) {
    Serial.print("No device available");
  }
  else {  
    sensors.requestTemperatures();
    
    unsigned long maxTime = millis() + sensors.millisToWaitForConversion(TEMPERATURE_PRECISION);
  
    do {
      // do other stuff
    } while(!sensors.isConversionAvailable(deviceAddress) && (millis() < maxTime));
    
    float tempC = sensors.getTempC(deviceAddress);
  
    Serial.println(tempC);
  }
  
  delay(1000);
}

