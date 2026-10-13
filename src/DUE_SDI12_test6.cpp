// INCLUDES:
#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>

// DEFINITIONS:
Adafruit_BME680 bme;
BH1750 lightMeter(0x23);
#define DIRO 7

// GLOBAL VARIABLES:
String command;
char deviceAddress = '0';
String deviceIdentification = "allccccccccmmmmmmvvvxxx";
int amountOfSensorsOnline = 0;
bool bmeExsists = false;
bool bhExsists = false;

// I2C SETUP:
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

// SETUP:
void setup() {
  Serial.begin(9600);
  Wire.begin();
  scanI2C();

  Serial1.begin(1200, SERIAL_7E1);
  pinMode(DIRO, OUTPUT);
  digitalWrite(DIRO, HIGH);
}

// FUNCTIONS:
void errorMessage() {
  Serial.println("Please return a valid response!!!");
  Serial.println("Valid Commands Include:");
  Serial.println("1. ?! - Address Query");
  Serial.println("2. aAb! - Change Address");
  Serial.println("3. aM! - Start Measurement");
  Serial.println("4. aD0!-aD9! - Send Data");
  Serial.println("5. aI! - Address Identification");
}

int activeSensorCount() {
  int count = 0;
  if (bmeExsists) {
    count += 4;
  }
  if (bhExsists) {
    count += 1;
  }
  return count;
}

void SDI12Send(String message) {
  Serial.print("message: ");
  Serial.println(message);

  digitalWrite(DIRO, LOW);
  delay(5);
  Serial1.print(message + "\r\n");
  Serial1.flush();
  delay(10);
  digitalWrite(DIRO, HIGH);

  while (Serial1.available()) {
    Serial1.read();
  }
  command = "";
}

void SDI12Receive(String input) {
  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  String address = String(deviceAddress);

  // The receive loop removes '!', so ?! arrives here as "?".
  if (input == "?") {
    SDI12Send(address);
    return;
  }

  if (input.length() != 3 || String(input.charAt(0)) != address) {
    if (input.length() != 5 || String(input.charAt(0)) != address || input.substring(1, 5) != "TEST") {
      errorMessage();
      return;
    }
  }

  if (input.length() == 5 && input.substring(1, 5) == "TEST") {
    bmeExsists = bme.begin(0x76);
    bhExsists = lightMeter.begin();

    if (bmeExsists) {
      bme.setTemperatureOversampling(BME680_OS_8X);
      bme.setPressureOversampling(BME680_OS_8X);
      bme.setHumidityOversampling(BME680_OS_2X);
    }

    if (!bhExsists) {
      Serial.println("BH1750 not found at 0x23 (try address 0x5C or check wiring)");
    }

    uint16_t lux = bhExsists ? lightMeter.readLightLevel() : 0;
    float temperature = bmeExsists && bme.performReading() ? bme.temperature : 0.0F;
    SDI12Send(address + "+" + String(temperature, 2) + "+" + String(lux));
    Serial.println("Responding to TEST command");
    return;
  }

  char inputValueCommandCharacter = input.charAt(1);
  switch (inputValueCommandCharacter) {
    case 'A': {
      char newAddress = input.charAt(2);
      bool validAddress = (newAddress >= '0' && newAddress <= '9') ||
                          (newAddress >= 'A' && newAddress <= 'Z') ||
                          (newAddress >= 'a' && newAddress <= 'z');
      if (!validAddress) {
        errorMessage();
        return;
      }

      deviceAddress = newAddress;
      SDI12Send(String(deviceAddress));
      Serial.print("Address changed to: ");
      Serial.println(deviceAddress);
      return;
    }

    case 'M': {
      bmeExsists = bme.begin(0x76);
      bhExsists = lightMeter.begin();

      if (bmeExsists) {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setPressureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
      }

      amountOfSensorsOnline = activeSensorCount();
      String response = address + "003" + String(amountOfSensorsOnline);
      Serial.println(response);
      SDI12Send(response);
      return;
    }

    case 'D': {
        switch (input.charAt(2)){
            case '0': {
              // Read the light level
              uint16_t lux = lightMeter.readLightLevel();

              // Send through to SDI-12
              SDI12Send(address + "+" + String(lux));

              // Confirm response
              Serial.println("Responding to LUX reading");
              return;
            }
            case '1': {
              // Read BME data
              // Returns accessors temperature, humidity, pressure (kPa), and gas resistance (mOhm)
              bme.performReading();

              // Format into payload
              String payload = address + "+" +
                String(bme.temperature, 1) +
                '+' + 
                String(bme.humidity, 1) + 
                '+' + 
                String(bme.pressure / 100, 1) + 
                '+' + 
                String(bme.gas_resistance / 1000, 1);

              // Pass payload into SDI-12
              SDI12Send(payload);
              
              // Confirm response
              Serial.println("Responding to BME reading");
              return;
            }
            case '2': {
              // Read sensors
              uint16_t lux = lightMeter.readLightLevel();
              bme.performReading();

              // Format into payload
              String payload = address + "+" +
                String(lux) +
                '+' +
                String(bme.temperature, 1) + 
                '+' + 
                String(bme.humidity, 1) + 
                '+' + 
                String(bme.pressure / 100, 1) + 
                '+' + 
                String(bme.gas_resistance / 1000, 1);
              
              // Pass payload into SDI-12
              SDI12Send(payload);

              // Confirm response
              Serial.println("Responding to sensor readings");
              return;
            }
            default: {
              errorMessage();
              return;
            }
          }
          break; }
    case 'I':
        {
        String identification = String(deviceAddress) + "14" + "ENG20009" + "105916092";
        SDI12Send(identification);
        return;
        }
      return;

    default:
      errorMessage();
      return;
  }
}

void loop() {
  while (Serial1.available()) {
    int receivedByte = Serial1.read();

    if (receivedByte == '!') {
      SDI12Receive(command);
      command = "";
    } else if (receivedByte != 0 && receivedByte != '\r' && receivedByte != '\n') {
      command += static_cast<char>(receivedByte);
    }
  }
}
