#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>

// ================= CONSTANTS & GLOBALS =================
#define DIRO 7

// Sensors Setup
Adafruit_BME680 bme;
BH1750 lightMeter(0x23);

// SDI-12 & State Variables
String command = "";
char deviceAddress = '0';          // SDI-12 addresses are typically chars ('0'-'9', 'a'-'z', 'A'-'Z')
String studentID = "123456";       // Replace with your actual student ID (first 6 chars used)

// Measurement Storage
bool measurementReady = false;
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
float gas = 0.0;
uint16_t lux = 0;

// Function Prototypes
void scanI2C();
void SDI12Send(String message);
void takeMeasurement();
void addressQuery();
void changeAddress(String input);
void startMeasurement();
void sendData(int dataNumber);
void sendIdentification();
void SDI12Receive(String input);

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  scanI2C();

  // BME680 Setup
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setPressureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);

  // BH1750 Setup
  if (!lightMeter.begin()) {
    Serial.println("BH1750 not found at 0x23 (try address 0x5C or check wiring)");
  }

  // SDI-12 Setup
  Serial1.begin(1200, SERIAL_7E1);  // 7 data bits, even parity, 1 stop bit
  pinMode(DIRO, OUTPUT);            // Direction pin for RS485/SDI-12 transceiver
  digitalWrite(DIRO, HIGH);         // HIGH to Receive
}

// ================= LOOP =================
void loop() {
  if (Serial1.available()) {
    int incomingByte = Serial1.read();
    
    if (incomingByte == 33) {       // '!' is the command terminator
      SDI12Receive(command);
      command = "";                 
    } else {
      // Ignore null, newline, and carriage return
      if (incomingByte != 0 && incomingByte != 10 && incomingByte != 13) {          
        command += char(incomingByte);
      }
    }
  }
}

// ================= I2C SCANNER =================
void scanI2C() {
  Serial.println("I2C scan:");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
}

// ================= SDI-12 TRANSMIT =================
void SDI12Send(String message) {
  Serial.print("message: \n"); 
  Serial.println(message);
  
  digitalWrite(DIRO, LOW);       // Enable TX
  Serial1.print(message + "\r\n");
  Serial1.flush();               // Wait for TX buffer to clear
  
  delay(10);                     // Wait for shift register to finish stop bits
  
  digitalWrite(DIRO, HIGH);      // Switch back to RX

  // Clear hardware buffer echo
  while (Serial1.available()) {
    Serial1.read();
  }
  command = ""; 
}

// ================= SENSOR MEASUREMENT =================
void takeMeasurement() {
  if (bme.performReading()) {
    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0;
    gas = bme.gas_resistance / 1000.0;
  }

  lux = lightMeter.readLightLevel();
  measurementReady = true;
}

// ================= ADDRESS QUERY (?!) =================
void addressQuery() {
  SDI12Send(String(deviceAddress));
}

// ================= CHANGE ADDRESS (aAb!) =================
void changeAddress(String input) {
  if (input.length() != 4) return;

  char oldAddress = input.charAt(0);
  char newAddress = input.charAt(2);

  if (input.charAt(1) != 'A') return;
  if (oldAddress != deviceAddress) return;

  // Validate SDI-12 address range (0-9, A-Z, a-z)
  if (!((newAddress >= '0' && newAddress <= '9') || 
        (newAddress >= 'A' && newAddress <= 'Z') || 
        (newAddress >= 'a' && newAddress <= 'z'))) {
    return;
  }

  deviceAddress = newAddress;
  SDI12Send(String(deviceAddress));

  Serial.print("Address changed to: ");
  Serial.println(deviceAddress);
}

// ================= START MEASUREMENT (aM!) =================
void startMeasurement() {
  takeMeasurement();

  // Response format: atttn (address, duration in seconds, number of values)
  String response = String(deviceAddress) + "00205";
  SDI12Send(response);
}

// ================= SEND DATA (aD0!) =================
void sendData(int dataNumber) {
  if (!measurementReady) return;

  if (dataNumber == 0) {
    String response = String(deviceAddress) +
                      String(temperature, 1) + "+" +
                      String(humidity, 1) + "+" +
                      String(pressure, 1) + "+" +
                      String(gas, 1) + "+" +
                      String(lux);

    SDI12Send(response);
  }
}

// ================= SEND IDENTIFICATION (aI!) =================
void sendIdentification() {
  // Format: a14ENG20009mmmmmmvvvxxx
  String response = String(deviceAddress) +
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

  if (input.length() == 0) return;

  // Address Query
  if (input == "?!") {
    addressQuery();
    return;
  }

  // All subsequent commands must start with our device address
  if (input.charAt(0) != deviceAddress) {
    return;
  }

  // Change Address (aAb!)
  if (input.length() == 4 && input.charAt(1) == 'A') {
    changeAddress(input);
    return;
  }

  // Start Measurement (aM!)
  if (input == String(deviceAddress) + "M!") {
    startMeasurement();
    return;
  }

  // Send Data (aD0! - aD9!)
  if (input.length() == 3 && input.charAt(1) == 'D' &&
      input.charAt(2) >= '0' && input.charAt(2) <= '9') {
    int dataNumber = input.charAt(2) - '0';
    sendData(dataNumber);
    return;
  }

  // Send Identification (aI!)
  if (input == String(deviceAddress) + "I!") {
    sendIdentification();
    return;
  }

  Serial.println("Unknown SDI-12 command");
}