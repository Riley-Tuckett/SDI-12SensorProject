#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>

// BME680 Setup
Adafruit_BME680 bme;

// BH1750 Setup
BH1750 lightMeter(0x23);

// SDI-12 Setup
#define DIRO 7

String command;
int deviceAddress = 0;
String deviceIdentification = "allccccccccmmmmmmvvvxxx";

void scanI2C() {
  Serial.println("I2C scan:");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
}

void setup() {
  // Arduino IDE Serial Monitor
  Serial.begin(9600);
  Wire.begin();
  scanI2C();

  // ================ BME680 ================
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }
  // Set the temperature, pressure and humidity oversampling
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setPressureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);

  // ================ BH1750 ================
  if (!lightMeter.begin()) {
    Serial.println("BH1750 not found at 0x23 (try address 0x5C or check wiring)");
  }

  // ================ SDI-12 ================
  Serial1.begin(1200, SERIAL_7E1);  // SDI-12 UART, configures serial port for 7 data bits, even parity, and 1 stop bit
  pinMode(DIRO, OUTPUT);            // DIRO Pin

  // HIGH to Receive from SDI-12
  digitalWrite(DIRO, HIGH);
}


void SDI12Send(String message) {
  Serial.print("message: \n"); 
  Serial.println(message);
  
  digitalWrite(DIRO, LOW);       // Enable TX
  Serial1.print(message + "\r\n");
  Serial1.flush();               // Wait for TX buffer
  
  // Wait 10ms for the hardware shift register to finish the stop bits
  delay(10); 
  
  digitalWrite(DIRO, HIGH);      // Switch back to RX
  

  // 1. Clear any bytes received in the hardware buffer while transmitting (echo)
  while (Serial1.available()) {
    Serial1.read();
  }
  // 2. Reset the software string buffer just to be safe
  command = ""; 
}

<<<<<<< HEAD
<<<<<<< Updated upstream
// This is the function that needs to be modified for the pass task
=======
// ================= SENSOR MEASUREMENT =================

void takeMeasurement() {

  // =================================================
  // TODO:
  // Put the actual BME680/BH1750 measurement code here.
  // Other group members can modify this function.
  // =================================================

  if (bme.performReading()) {

    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0;
    gas = bme.gas_resistance / 1000.0;

  }

  lux = lightMeter.readLightLevel();

  measurementReady = true;
}


// ================= ADDRESS QUERY =================

void addressQuery() {

  // ?!
  SDI12Send(String(deviceAddress));
}


// ================= CHANGE ADDRESS =================
void changeAddress(String input) {
  // Expected:
  // aAb!
  //
  // Example:
  // 0A1!
  //
  // Changes address from 0 -> 1

  if (input.length() != 4) {
    return;
  }

  char oldAddress = input.charAt(0);
  char newAddress = input.charAt(2);

  // Make sure command is actually A
  if (input.charAt(1) != 'A') {
    return;
  }

  // Make sure command is being sent to our current address
  if (oldAddress != deviceAddress) {
    return;
  }

  // SDI-12 addresses are 0-9, A-Z, a-z
  // This is a simple validation for now.
  if (!((newAddress >= '0' && newAddress <= '9') || (newAddress >= 'A' && newAddress <= 'Z') || (newAddress >= 'a' && newAddress <= 'z'))) {
    return;
  }

  // Change address
  deviceAddress = newAddress;

  // Response is the NEW address
  SDI12Send(String(deviceAddress));

  Serial.print("Address changed to: ");
  Serial.println(deviceAddress);
}


// ================= START MEASUREMENT =================

void startMeasurement() {

  // =================================================
  // TODO:
  // Other group member can implement measurement timing.
  //
  // Response format:
  //
  // atttn
  //
  // Example:
  // 000205
  //
  // 0 = address
  // 002 = 2 seconds
  // 05 = 5 measurements
  // =================================================

  takeMeasurement();

  String response =
    String(deviceAddress) +
    "00205";

  SDI12Send(response);
}


// ================= SEND DATA =================

void sendData(int dataNumber) {

  // =================================================
  // TODO:
  // Implement D0-D9 data packets here.
  //
  // Required values:
  // Temperature
  // Humidity
  // Pressure
  // Gas
  // Lux
  //
  // Example:
  // 0+24.5+55.0+1013.2+450.0
  // =================================================

  if (!measurementReady) {
    return;
  }

  if (dataNumber == 0) {

    String response =
      String(deviceAddress) +
      String(temperature, 1) +
      "+" + String(humidity, 1) +
      "+" + String(pressure, 1) +
      "+" + String(gas, 1) +
      "+" + String(lux, 1);

    SDI12Send(response);
  }

  // -------------------------------------------------
  // TODO:
  // D1-D9 can be implemented here if more than one
  // data packet is required.
  // -------------------------------------------------
}


// ================= SEND IDENTIFICATION =================

void sendIdentification() {

  // =================================================
  // TODO:
  // Insert student ID here.
  //
  // Required format:
  //
  // a14ENG20009mmmmmmvvvxxx
  //
  // where:
  // a        = address
  // 14       = SDI-12 v1.4
  // ENG20009 = manufacturer
  // mmmmmm   = first 6 characters of student ID
  // vvv      = next 3 characters
  // xxx      = can be ignored
  // =================================================

  String response =
    String(deviceAddress) +
    "14" +
    "ENG20009" +
    studentID +
    "xxx";

  SDI12Send(response);
}


// ================= SDI-12 COMMAND PARSER =================

>>>>>>> Stashed changes
void SDI12Receive(String input) {
  Serial.print("Received SDI-12 command: ");
  Serial.println(input);
=======
// ================= SENSOR MEASUREMENT =================
>>>>>>> a596775cf71c23d64d7d4f7208c2d1d82b6bd27e

void takeMeasurement() {

  // =================================================
  // TODO:
  // Put the actual BME680/BH1750 measurement code here.
  // Other group members can modify this function.
  // =================================================

  if (bme.performReading()) {

    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0;
    gas = bme.gas_resistance / 1000.0;

  }

  lux = lightMeter.readLightLevel();

  measurementReady = true;
}


// ================= ADDRESS QUERY =================

void addressQuery() {

  // ?!
  SDI12Send(String(deviceAddress));
}


// ================= CHANGE ADDRESS =================

void changeAddress(String input) {

  // Expected:
  // aAb!
  //
  // Example:
  // 0A1!
  //
  // Changes address from 0 -> 1

  if (input.length() != 4) {
    return;
  }

<<<<<<< HEAD
<<<<<<< Updated upstream
  String address = String(deviceAddress);
  
  if (String(input.charAt(0)) == address) {  
    if (input.substring(1, 5) == "TEST") {  // Listen for a specific string of characters. This can be anything.
=======

  // =================================================
  // ADDRESS QUERY
  // ?!
  // =================================================

  if (input == "?!") {

    addressQuery();
    return;
  }


  // =================================================
  // COMMANDS MUST START WITH OUR ADDRESS
  // =================================================

  if (input.charAt(0) != deviceAddress) {
    return;
  }


  // =================================================
  // CHANGE ADDRESS
  // aAb!
  // =================================================

  if (input.length() == 4 && input.charAt(1) == 'A') {
    changeAddress(input);
    return;
  }


  // =================================================
  // START MEASUREMENT
  // aM!
  // =================================================

  if (input == String(deviceAddress) + "M!") {

    startMeasurement();
    return;
  }


  // =================================================
  // SEND DATA
  // aD0! ... aD9!
  // =================================================

  if (input.length() == 3 &&
      input.charAt(1) == 'D' &&
      input.charAt(2) >= '0' &&
      input.charAt(2) <= '9') {

    int dataNumber = input.charAt(2) - '0';

    sendData(dataNumber);
    return;
  }


  // =================================================
  // SEND IDENTIFICATION
  // aI!
  // =================================================

  if (input == String(deviceAddress) + "I!") {

    sendIdentification();
    return;
  }


  // =================================================
  // UNKNOWN COMMAND
  // =================================================

  Serial.println("Unknown SDI-12 command");
}

// This is the function that needs to be modified for the pass task
// void SDI12Receive(String input) {
//   Serial.print("Received SDI-12 command: ");
//   Serial.println(input);

//   if (input.length() < 5) {
//     return;
//   }

//   String address = String(deviceAddress);

//   if (String(input.charAt(0)) == address) {  
//     if (input.substring(1, 5) == "TEST") {  // Listen for a specific string of characters. This can be anything.
>>>>>>> Stashed changes
      
      // Execute code needed on command invocation
      uint16_t lux = lightMeter.readLightLevel();
      bme.performReading();
      float temp = bme.temperature;
=======
  char oldAddress = input.charAt(0);
  char newAddress = input.charAt(2);
>>>>>>> a596775cf71c23d64d7d4f7208c2d1d82b6bd27e

  // Make sure command is actually A
  if (input.charAt(1) != 'A') {
    return;
  }

  // Make sure command is being sent to our current address
  if (oldAddress != deviceAddress) {
    return;
  }

  // SDI-12 addresses are 0-9, A-Z, a-z
  // This is a simple validation for now.
  if (!(
      (newAddress >= '0' && newAddress <= '9') ||
      (newAddress >= 'A' && newAddress <= 'Z') ||
      (newAddress >= 'a' && newAddress <= 'z')
    )) {
    return;
  }

  // Change address
  deviceAddress = newAddress;

  // Response is the NEW address
  SDI12Send(String(deviceAddress));

  Serial.print("Address changed to: ");
  Serial.println(deviceAddress);
}


// ================= START MEASUREMENT =================

void startMeasurement() {

  // =================================================
  // TODO:
  // Other group member can implement measurement timing.
  //
  // Response format:
  //
  // atttn
  //
  // Example:
  // 000205
  //
  // 0 = address
  // 002 = 2 seconds
  // 05 = 5 measurements
  // =================================================

  takeMeasurement();

  String response =
    String(deviceAddress) +
    "00205";

  SDI12Send(response);
}


// ================= SEND DATA =================

void sendData(int dataNumber) {

  // =================================================
  // TODO:
  // Implement D0-D9 data packets here.
  //
  // Required values:
  // Temperature
  // Humidity
  // Pressure
  // Gas
  // Lux
  //
  // Example:
  // 0+24.5+55.0+1013.2+450.0
  // =================================================

  if (!measurementReady) {
    return;
  }

  if (dataNumber == 0) {

    String response =
      String(deviceAddress) +
      String(temperature, 1) +
      "+" + String(humidity, 1) +
      "+" + String(pressure, 1) +
      "+" + String(gas, 1) +
      "+" + String(lux, 1);

    SDI12Send(response);
  }

  // -------------------------------------------------
  // TODO:
  // D1-D9 can be implemented here if more than one
  // data packet is required.
  // -------------------------------------------------
}


// ================= SEND IDENTIFICATION =================

void sendIdentification() {

  // =================================================
  // TODO:
  // Insert student ID here.
  //
  // Required format:
  //
  // a14ENG20009mmmmmmvvvxxx
  //
  // where:
  // a        = address
  // 14       = SDI-12 v1.4
  // ENG20009 = manufacturer
  // mmmmmm   = first 6 characters of student ID
  // vvv      = next 3 characters
  // xxx      = can be ignored
  // =================================================

  String response =
    String(deviceAddress) +
    "14" +
    "ENG20009" +
    studentID +
    "xxx";

  SDI12Send(response);
}


// ================= SDI-12 COMMAND PARSER =================

void SDI12Receive(String input) {

  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  if (input.length() == 0) {
    return;
  }


  // =================================================
  // ADDRESS QUERY
  // ?!
  // =================================================

  if (input == "?!") {

    addressQuery();
    return;
  }


  // =================================================
  // COMMANDS MUST START WITH OUR ADDRESS
  // =================================================

  if (input.charAt(0) != deviceAddress) {
    return;
  }


  // =================================================
  // CHANGE ADDRESS
  // aAb!
  // =================================================

  if (input.length() == 4 &&
      input.charAt(1) == 'A') {

    changeAddress(input);
    return;
  }


  // =================================================
  // START MEASUREMENT
  // aM!
  // =================================================

  if (input == String(deviceAddress) + "M!") {

    startMeasurement();
    return;
  }


  // =================================================
  // SEND DATA
  // aD0! ... aD9!
  // =================================================

  if (input.length() == 3 &&
      input.charAt(1) == 'D' &&
      input.charAt(2) >= '0' &&
      input.charAt(2) <= '9') {

    int dataNumber = input.charAt(2) - '0';

    sendData(dataNumber);
    return;
  }


  // =================================================
  // SEND IDENTIFICATION
  // aI!
  // =================================================

  if (input == String(deviceAddress) + "I!") {

    sendIdentification();
    return;
  }


  // =================================================
  // UNKNOWN COMMAND
  // =================================================

  Serial.println("Unknown SDI-12 command");
}

// This is the function that needs to be modified for the pass task
// void SDI12Receive(String input) {
//   Serial.print("Received SDI-12 command: ");
//   Serial.println(input);

//   if (input.length() < 5) {
//     return;
//   }

//   String address = String(deviceAddress);

//   if (String(input.charAt(0)) == address) {  
//     if (input.substring(1, 5) == "TEST") {  // Listen for a specific string of characters. This can be anything.
      
//       // Execute code needed on command invocation
//       uint16_t lux = lightMeter.readLightLevel();
//       bme.performReading();
//       float temp = bme.temperature;

//       // Copy this format for a response. 
//       // Create the human-readable string without the '0' address
//       String payload = "temperature: " + String(temp, 2) + " \n\rlux: " + String(lux);

//       SDI12Send(payload);
//       Serial.println("Responding to TEST command");
//     }
//   } 
// }

void loop() {
  int byte;
  if (Serial1.available()) {
    byte = Serial1.read();
    
    if (byte == 33) {               // If byte is command terminator (!)
      SDI12Receive(command);
      command = "";                 // reset command string
    } else {
      // Ignore start bit (0), newline (10), and carriage return (13)
      if (byte != 0 && byte != 10 && byte != 13) {          
        command += char(byte);
      }
    }
  }
}