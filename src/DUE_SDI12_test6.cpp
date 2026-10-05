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

// This is the function that needs to be modified for the pass task
void SDI12Receive(String input) {
  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  if (input.length() < 5) {
    return;
  }

  String address = String(deviceAddress);
  
  if (String(input.charAt(0)) == address) {  
    if (input.substring(1, 5) == "TEST") {  // Listen for a specific string of characters. This can be anything.
      
      // Execute code needed on command invocation
      uint16_t lux = lightMeter.readLightLevel();
      bme.performReading();
      float temp = bme.temperature;

      // Copy this format for a response. 
      // Create the human-readable string without the '0' address
      String payload = "temperature: " + String(temp, 2) + " \n\rlux: " + String(lux);

      SDI12Send(payload);
      Serial.println("Responding to TEST command");
    }
  } 
}

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