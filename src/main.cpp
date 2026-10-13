// INCLUDES:
#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>
#include <RTClib.h>
#include <SdFat.h>


// DEFINITIONS:
#define DIRO 7

// SENSOR SETUP:
Adafruit_BME680 bme;
BH1750 lightMeter(0x23);

// GLOBAL VARIABLES:
String command;
char deviceAddress = '0';
String deviceIdentification = "14ENG20009105916092";
String measurements[5];
uint8_t measurementCount = 0;
bool bmeExists = false;
bool bhExists = false;
bool measurementReady = false;

//SD CARD SETUP:
const uint8_t SD_CS = 10;
SdFs sd;
FsFile dataLOG;
SoftSpiDriver<12, 11, 13> softSpi;

//SPECIAL SETUP:
RTC_DS1307 rtc;
const auto SD_CONFIG = SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(0), &softSpi);

//TIME:
long unsigned millisBefore = 0;
const long interval = 30000;
int hour = 0;
int minute = 0;
int seconds = 0;

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

//This is a very cool error message :).
void errorMessage() {
  Serial.println("Please return a valid response!!!");
  Serial.println("Valid Commands Include:");
  Serial.println("1. ?! - Address Query");
  Serial.println("2. aAb! - Change Address");
  Serial.println("3. aM! - Start Measurement");
  Serial.println("4. aD0!-aD9! - Send Data");
  Serial.println("5. aI! - Address Identification");
}


void SDI12Send(String message) {
  Serial.print("message: ");
  Serial.println(message);

  digitalWrite(DIRO, LOW);
  delay(5);
  Serial1.print(message);
  Serial1.print("\r\n");
  Serial1.flush();
  delay(10);
  digitalWrite(DIRO, HIGH);

  while (Serial1.available()) {
    Serial1.read();
  }
  command = "";
}

void initializeSensors() {
  bmeExists = bme.begin(0x76);
  if (bmeExists) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setPressureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
  } else {
    Serial.println("BME680 not found at 0x76");
  }

  bhExists = lightMeter.begin();
  if (!bhExists) {
    Serial.println("BH1750 not found at 0x23");
  }
}

void readSensors() {
  measurementCount = 0;
  measurementReady = false;

  if (bmeExists && bme.performReading()) {
    measurements[measurementCount++] = String(bme.temperature, 1);
    measurements[measurementCount++] = String(bme.humidity, 1);
    measurements[measurementCount++] = String(bme.pressure / 100.0F, 1);
    measurements[measurementCount++] = String(bme.gas_resistance / 1000.0F, 1);
  }

  if (bhExists) {
    measurements[measurementCount++] = String(lightMeter.readLightLevel(), 1);
  }

  measurementReady = measurementCount > 0;
}

String measurementResponse() {
  String response = String(deviceAddress);
  for (uint8_t index = 0; index < measurementCount; index++) {
    float value = measurements[index].toFloat();
    if (value >= 0) {
      response += "+";
    }
    response += measurements[index];
  }
  return response;
}

void SDI12Receive(String input) {
  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  // The receive loop removes !, so ?! arrives as "?".
  if (input == "?") {
    SDI12Send(String(deviceAddress));
    return;
  }

  if (input.length() < 2 || input.charAt(0) != deviceAddress) {
    errorMessage();
    return;
  }

  char commandType = input.charAt(1);

  switch (commandType) {
    // Change address: aAb!
    case 'A': {
      if (input.length() != 3) {
        errorMessage();
        return;
      }

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

    // Start measurement: aM!
    case 'M': {
      if (input.length() != 2) {
        errorMessage();
        return;
      }

      initializeSensors();
      readSensors();
      // Measurements are read immediately; 002 gives the host time to process the response.
      String response = String(deviceAddress) + "002";
      if (measurementCount < 10) {
        response += "0";
      }
      response += String(measurementCount);
      SDI12Send(response);
      return;
    }

    // Send data: aD0! through aD9!
    case 'D': {
      if (input.length() != 3 || input.charAt(2) < '0' || input.charAt(2) > '9') {
        errorMessage();
        return;
      }

      switch (input.charAt(2)) {
        case '0': {
          if (!bhExists) {
            errorMessage();
            return;
          }

          String response = String(deviceAddress) + "+" + String(lightMeter.readLightLevel(), 1);
          SDI12Send(response);
          return;
        }

        case '1': {
          if (!bmeExists || !bme.performReading()) {
            errorMessage();
            return;
          }

          String response = String(deviceAddress) +
                            "+" + String(bme.temperature, 1) +
                            "+" + String(bme.humidity, 1) +
                            "+" + String(bme.pressure / 100.0F, 1) +
                            "+" + String(bme.gas_resistance / 1000.0F, 1);
          SDI12Send(response);
          return;
        }

        case '2':
          readSensors();
          if (!measurementReady) {
            errorMessage();
            return;
          }
          SDI12Send(measurementResponse());
          return;

        default:
          errorMessage();
          return;
      }
    }

    // Send identification: aI!
    case 'I':
      if (input.length() != 2) {
        errorMessage();
        return;
      }
      SDI12Send(String(deviceAddress) + deviceIdentification);
      return;

    default:
      errorMessage();
      return;
  }
}

void ReadRTC(){
  DateTime now = rtc.now();
  int hours = now.hour();
  int minutes = now.minute();
  int seconds = now.second();
}

void ReadDataToSD(){

}

void WriteDataToSD(){
  if (dataLOG == true){
    //If the file is brand new, write a CSV header row.
    if (dataLOG.size() == 0) {
      dataLOG.println(F("temp,humidity,gas,lux,hourRTC,minuteRTC,secondRTC"));
      dataLOG.sync(); //Save the header immediately.
    }
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  scanI2C();

  //THE RTC BEGINSSSSSSS!!!!!!!!!
  rtc.begin();
  rtc.adjust(DateTime(2026, 9, 18, 11, 59, 50));


  Serial.println("Initializing SD card using Software SPI...");

  //Configure the SD card Chip Select pin.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  //Initialize the SD card using the Software SPI configuration.
  if (!sd.begin(SD_CONFIG)) {
    Serial.println("SD card initialization failed! Check wiring or card format (FAT32).");
    return;
  }
  
  Serial.println("SD card initialized successfully!");

  //Open or create "LOG.CSV" in read/write/append mode.
  dataLOG = sd.open("LOG.CSV", O_RDWR | O_CREAT | O_APPEND);
  if (dataLOG == true){
    //If the file is brand new, write a CSV header row.
    if (dataLOG.size() == 0) {
      dataLOG.println(F("temp,humidity,gas,lux,hourRTC,minuteRTC,secondRTC"));
      dataLOG.sync(); //Save the header immediately.
    }
  //Close the file when done writing.
    flightLog.close();
    
    Serial.println(F("Test data written to FLIGHT.CSV successfully!"));
  } else {
    Serial.println(F("Error: Could not open FLIGHT.CSV for writing."));
  }

  Serial1.begin(1200, SERIAL_7E1);
  pinMode(DIRO, OUTPUT);
  digitalWrite(DIRO, HIGH);
  Serial.println("SDI-12 ready");
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
