#include <Arduino.h>
#include <Wire.h>

/*
  HYT131 I2C address change

  Changes the I2C slave address of a HYT-131 temperature/humidity
  sensor.
  Based on information found in the ZSSC3122 datasheet.

  Connections:
  - VCC to the pin defined under POWER_PIN
  - SDA to analog pin 4
  - SCL to analog pin 5
  - GND to ground
*/

#define SENSOR_ADDR 0x29
#define SENSOR_NEW_ADDR 0x28

#define POWER_PIN 12

/* Don't change anything below here */

#define CMD_START_CM 0xA0
#define CMD_START_NM 0x80
#define RESP_ACK_POS 1
#define RESP_ACK_NEG 2
#define RESP_CMD_MODE 1<<7
#define ZMDI_COMM_LOCK 0x300

/*
  enterCmdMode(char dev_addr)
  Tries to boot the device at dev_addr into command mode.
*/
bool enterCmdMode(char dev_addr) {
  // turn the sensor on
  digitalWrite(POWER_PIN, HIGH);
  delay(2);

  // send Star_CM command
  Wire.beginTransmission(dev_addr);
  Wire.write(CMD_START_CM);
  Wire.write(0);
  Wire.write(0);
  return Wire.endTransmission();
}

/*
  enterNomMode(char dev_addr)
  Tries to change the device at dev_addr to normal mode.
*/
bool enterNomMode(char dev_addr) {
  // send Start_NM command
  Wire.beginTransmission(dev_addr);
  Wire.write(CMD_START_NM);
  Wire.write(0);
  Wire.write(0);
  return Wire.endTransmission();
}

/*
  readEEPROM(char dev_addr, byte address)
  Reads the EEPROM at address from device dev_addr.
*/
int readEEPROM(char dev_addr, byte address) {
  // check address
  if (address > 0x1F)
    return 0;

  // send EEPROM read command
  Wire.beginTransmission(dev_addr);
  Wire.write(address);
  Wire.write(0);
  Wire.write(0);
  Wire.endTransmission();
  delayMicroseconds(150);

  // retrieve data
  Wire.requestFrom(dev_addr, 3);
  byte hi, lo;
  Wire.read();
  hi = Wire.read();
  lo = Wire.read();

  return (hi<<8) | lo;
}

/*
  writeEEPROM(char dev_addr, byte address, int data)
  Writes data to the EEPROM at address of device dev_addr.
*/
void writeEEPROM(char dev_addr, byte address, int data) {
  // check address
  if (address > 0x1F)
    return;

  // send data
  Wire.beginTransmission(dev_addr);
  Wire.write(0x40 + address);
  Wire.write((char) (data>>8));
  Wire.write((char) data);
  Wire.endTransmission();
  delay(15);
}

/*
  changeAddr(char dev_old_addr, char dev_new_addr)
  Changes the device address of dev_addr to dev_new_addr.
*/
void changeAddr(char dev_addr, byte dev_new_addr) {
  int cust_config = readEEPROM(dev_addr, 0x1C);
  cust_config = (cust_config & 0xFF80) | dev_new_addr;
  writeEEPROM(dev_addr, 0x1C, cust_config);
}

/*
  changeCommLock(char dev_addr, byte commlock)
  Turns the Comm_lock in register ZMDI_config on or off.
*/
void changeCommLock(char dev_addr, byte commlock) {
  int zmdi_config = readEEPROM(dev_addr, 0x02);

  if (commlock && !(zmdi_config & ZMDI_COMM_LOCK)) {
    zmdi_config = (zmdi_config & ~0x700) | ZMDI_COMM_LOCK;
    writeEEPROM(dev_addr, 0x02, zmdi_config);
  }

  if (!commlock && (zmdi_config & ZMDI_COMM_LOCK)) {
    zmdi_config = zmdi_config & ~0x700;
    writeEEPROM(dev_addr, 0x02, zmdi_config);
  }
}

/*
  measHumidity(char dev_addr)
  Polls the device dev_addr and returns the measured humidity.
*/
int measHumidity(char dev_addr) {
  int h;

  Wire.beginTransmission(dev_addr);
  if (Wire.endTransmission())
    return 0xFFFF;
  delay(100);

  Wire.requestFrom(dev_addr, 2);
  h = (Wire.read() & 0x3F) << 8;
  h = h | Wire.read();
  return h * 10000L >> 14;
}


void setup(void) {
  int zmdi_config, cust_config;
  byte a;

  Serial.begin(57600);

  // set POWER_PIN as output
  digitalWrite(POWER_PIN, LOW);
  pinMode(POWER_PIN, OUTPUT);

  // setup Wire as master
  Wire.begin();

  // Wait for user interaction
  Serial.println("Press any key to start the procedure...");
  while(!Serial.available()) delay(1);

  // Try to enter command mode
  Serial.println("Entering command mode...");
  if (!enterCmdMode(SENSOR_ADDR)) {
    Serial.println("Command sent successfully.");
    delayMicroseconds(150);
  } else {
    Serial.println("Error!");
    goto out;
  }

  // Check if command was successful
  Wire.requestFrom(SENSOR_ADDR, 2);
  a = Wire.read();
  Wire.read();
  if (a & RESP_CMD_MODE) {
    Serial.println("Entered command mode.");
  } else {
    Serial.println("Error!");
    goto out;
  }

  // Retrieve ZMDI_Config
  Serial.println("Retrieve ZMDI_Config...");
  zmdi_config = readEEPROM(SENSOR_ADDR, 0x02);
  Serial.print("ZMDI_Config: 0x");
  Serial.println(zmdi_config, HEX);
  if (zmdi_config & ZMDI_COMM_LOCK)
    Serial.println("ZMDI_Config: Comm locked");
  else
    Serial.println("ZMDI_Config: Comm unlocked");

  // Retrieve Cust_Config
  Serial.println("Retrieve Cust_Config...");
  cust_config = readEEPROM(SENSOR_ADDR, 0x1C);
  Serial.print("Cust_Config: 0x");
  Serial.println(cust_config, HEX);
  Serial.print("I2C-ID: 0x");
  Serial.println(cust_config & 0x7F, HEX);

  // Enable Comm_lock
  Serial.println("Enable Comm_lock");
  changeCommLock(SENSOR_ADDR, 1);

  // Change address
  Serial.print("Change address from 0x");
  Serial.print(SENSOR_ADDR, HEX);
  Serial.print(" to 0x");
  Serial.println(SENSOR_NEW_ADDR, HEX);
  changeAddr(SENSOR_ADDR, SENSOR_NEW_ADDR);

  // Enter normal operation
  Serial.println("Entering normal operation...");
  if (!enterNomMode(SENSOR_ADDR)) {
    Serial.println("Command sent successfully.");
  } else {
    Serial.println("Error!");
    goto out;
  }
  Wire.requestFrom(SENSOR_NEW_ADDR, 2);
  a = Wire.read();
  Wire.read();
  if (!(a & RESP_CMD_MODE)) {
    Serial.println("Entered normal mode successfully.");
  } else {
    Serial.println("Error!");
  }

  delay(500);

  out:
  Serial.println("Finished.");
}

void loop(void) {
  // Display humidity
  int humidity = measHumidity(SENSOR_NEW_ADDR);
  Serial.print("Humidity: ");
  Serial.print((float) humidity / 100, 2);
  Serial.println("%relF");
  delay(1000);
}
