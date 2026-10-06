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
char deviceAddress = '0'; 
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
   
 
  // Clear any bytes received in the hardware buffer while transmitting (echo) 
  while (Serial1.available()) { 
    Serial1.read(); 
  } 
  // Reset the software string buffer just to be safe 
  command = "";  
} 
 
// This is the function that needs to be modified for the pass task
void SDI12Receive(String input) {

  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  String address = String(deviceAddress);


  // =========================================================
  // PASS LEVEL: ADDRESS QUERY
  // Command: ?!
  // Response: a<CR><LF>
  //
  // Returns the current address of the sensor.
  // Default address is 0.
  // =========================================================

  if (input == "?") {
    SDI12Send(address);
    return;
  }


  // =========================================================
  // PASS LEVEL: CHANGE ADDRESS
  // Command: aAb!
  // Response: b<CR><LF>
  //
  // Example:
  // 0A1!
  //
  // Changes the sensor address from 0 to 1.
  // =========================================================

  if (input.length() == 3) {

    // Check that the command is addressed to this sensor
    if (String(input.charAt(0)) == address) {

      // Check that the command is an Address Change command
      if (input.charAt(1) == 'A') {

        char newAddress = input.charAt(2);

        // Check that the new address is valid
        if ((newAddress >= '0' && newAddress <= '9') || (newAddress >= 'A' && newAddress <= 'Z') || (newAddress >= 'a' && newAddress <= 'z')) {
          // Change the address
          deviceAddress = newAddress;

          // Respond with the new address
          SDI12Send(String(deviceAddress));
          Serial.print("Address changed to: ");
          Serial.println(deviceAddress);
        }
      }
    }

    return;
  }


  // =========================================================
  // PASS LEVEL: START MEASUREMENT
  // Command: aM!
  // Response: atttn<CR><LF>
  //
  // TODO: Other team member
  //
  // Example:
  // 0M!
  // Response:
  // 000205
  //
  // ttt = measurement time
  // n   = number of values
  // =========================================================



  // =========================================================
  // PASS LEVEL: SEND DATA
  // Commands: aD0! - aD9!
  //
  // TODO: Other team member
  //
  // Expected values:
  // Temperature
  // Humidity
  // Pressure
  // Gas
  // Lux
  // =========================================================



  // =========================================================
  // PASS LEVEL: SEND IDENTIFICATION
  // Command: aI!
  //
  // TODO: Other team member
  //
  // Format:
  // a14ENG20009mmmmmmvvvxxx
  //
  // a        = sensor address
  // 14       = SDI-12 version
  // ENG20009 = manufacturer
  // Student ID
  // xxx      = ignored
  // =========================================================



  // =========================================================
  // EXISTING TEST COMMAND
  // Keep this for testing until the other Pass functions
  // are implemented.
  // =========================================================

  if (String(input.charAt(0)) == address) {

    if (input.substring(1, 5) == "TEST") {

      // Execute code needed on command invocation
      uint16_t lux = lightMeter.readLightLevel();

      bme.performReading();

      float temp = bme.temperature;

      String payload =
        String(temp, 2) +
        '+' +
        String(lux);

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