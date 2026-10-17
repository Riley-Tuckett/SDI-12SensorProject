// INCLUDES:
#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>


// Button Configs
const int BUTTON1_PIN = 2;
const int BUTTON2_PIN = 3;

const unsigned long DEBOUNCE_MS = 50;

// For button1 debounce
int button1LastReading = HIGH;
int button1StableState = HIGH;
unsigned long button1LastChange = 0;

//Button 2 debounce state
int button2LastReading = HIGH;
int button2StableState = HIGH;
unsigned long button2LastChange = 0;

// For current dashboard screen state
bool showSystemInfo = false;    //RUBY USE THIS VARIABLE PLS


// DEFINITIONS:
#define DIRO 8
#define TFT_CS 10
#define TFT_RST 6
#define TFT_DC 7
#define TFT_SCLK 13
#define TFT_MOSI 11

Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

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
unsigned long previousDisplayUpdate = 0;
const unsigned long displayInterval = 1000;
unsigned long previousButtonDebug = 0;

void scanI2C()
{
  Serial.println("I2C scan:");
  for (uint8_t address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("Found 0x");
      if (address < 16)
      {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
}

void errorMessage()
{
  Serial.println("Please return a valid response!!!");
  Serial.println("Valid Commands Include:");
  Serial.println("1. ?! - Address Query");
  Serial.println("2. aAb! - Change Address");
  Serial.println("3. aM! - Start Measurement");
  Serial.println("4. aD0!-aD9! - Send Data");
  Serial.println("5. aI! - Address Identification");
}

void SDI12Send(String message)
{
  Serial.print("message: ");
  Serial.println(message);

  digitalWrite(DIRO, LOW);
  delay(5);
  Serial1.print(message);
  Serial1.print("\r\n");
  Serial1.flush();
  delay(10);
  digitalWrite(DIRO, HIGH);

  while (Serial1.available())
  {
    Serial1.read();
  }
  command = "";
}

void initializeSensors()
{
  bmeExists = bme.begin(0x76);
  if (bmeExists)
  {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setPressureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
  }
  else
  {
    Serial.println("BME680 not found at 0x76");
  }

  bhExists = lightMeter.begin();
  if (!bhExists)
  {
    Serial.println("BH1750 not found at 0x23");
  }
}

void readSensors()
{
  measurementCount = 0;
  measurementReady = false;

  if (bmeExists && bme.performReading())
  {
    measurements[measurementCount++] = String(bme.temperature, 1);
    measurements[measurementCount++] = String(bme.humidity, 1);
    measurements[measurementCount++] = String(bme.pressure / 100.0F, 1);
    measurements[measurementCount++] = String(bme.gas_resistance / 1000.0F, 1);
  }

  if (bhExists)
  {
    measurements[measurementCount++] = String(lightMeter.readLightLevel(), 1);
  }

  measurementReady = measurementCount > 0;
}

String measurementResponse()
{
  String response = String(deviceAddress);
  for (uint8_t index = 0; index < measurementCount; index++)
  {
    float value = measurements[index].toFloat();
    if (value >= 0)
    {
      response += "+";
    }
    response += measurements[index];
  }
  return response;
}

void SDI12Receive(String input)
{
  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  // The receive loop removes !, so ?! arrives as "?".
  if (input == "?")
  {
    SDI12Send(String(deviceAddress));
    return;
  }

  if (input.length() < 2 || input.charAt(0) != deviceAddress)
  {
    errorMessage();
    return;
  }

  char commandType = input.charAt(1);

  switch (commandType)
  {
  // Change address: aAb!
  case 'A':
  {
    if (input.length() != 3)
    {
      errorMessage();
      return;
    }

    char newAddress = input.charAt(2);
    bool validAddress = (newAddress >= '0' && newAddress <= '9') ||
                        (newAddress >= 'A' && newAddress <= 'Z') ||
                        (newAddress >= 'a' && newAddress <= 'z');
    if (!validAddress)
    {
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
  case 'M':
  {
    if (input.length() != 2)
    {
      errorMessage();
      return;
    }

    initializeSensors();
    readSensors();
    // Measurements are read immediately; 002 gives the host time to process the response.
    String response = String(deviceAddress) + "002";
    if (measurementCount < 10)
    {
      response += "0";
    }
    response += String(measurementCount);
    SDI12Send(response);
    return;
  }

  // Send data: aD0! through aD9!
  case 'D':
  {
    if (input.length() != 3 || input.charAt(2) < '0' || input.charAt(2) > '9')
    {
      errorMessage();
      return;
    }

    switch (input.charAt(2))
    {
    case '0':
    {
      if (!bhExists)
      {
        errorMessage();
        return;
      }

      String response = String(deviceAddress) + "+" + String(lightMeter.readLightLevel(), 1);
      SDI12Send(response);
      return;
    }

    case '1':
    {
      if (!bmeExists || !bme.performReading())
      {
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
      if (!measurementReady)
      {
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
    if (input.length() != 2)
    {
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

void displayDashboard()
{
  display.fillScreen(ST77XX_BLACK);

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("Sensor Dashboard");

  display.setCursor(5, 25);
  display.print("Temp: ");
  display.print(measurements[0]);
  display.println(" C");

  display.setCursor(5, 35);
  display.print("Humidity: ");
  display.print(measurements[1]);
  display.println(" %");

  display.setCursor(5, 45);
  display.print("Pressure: ");
  display.print(measurements[2]);
  display.println(" hPa");

  display.setCursor(5, 55);
  display.print("Gas Resistance: ");
  display.print(measurements[3]);
  display.println(" kOhm");

  display.setCursor(5, 65);
  display.print("Light: ");
  display.print(measurements[4]);
  display.println(" lux");
}

// Button debouncing logic
bool buttonPressed(int pin,int &lastReading,int &stableState,unsigned long &lastChange) {
  int reading = digitalRead(pin);
  bool pressed = (lastReading == HIGH && reading == LOW);

  lastReading = reading;
  stableState = reading;
  lastChange = millis();

  return pressed;
}

//Handle da buttons
void handleButtons(){

  //Button 1: Manual logging
  if (buttonPressed(BUTTON1_PIN,button1LastReading,button1StableState,button1LastChange)){
    Serial.println("Button 1 pressed: Manual log reuested");
  }

  //Button 2: To toggle dashboard screen
  if (buttonPressed(BUTTON2_PIN,button2LastReading,button2StableState,button2LastChange)){
    showSystemInfo = !showSystemInfo;

    if (showSystemInfo){
      Serial.println("Screen: System info");
    } else{
      Serial.println("Screen: LIVE DATA");  //Once we get the lcd function to change display can put in here
    }
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  Wire.begin();
  scanI2C();
  initializeSensors();

  Serial1.begin(1200, SERIAL_7E1);
  pinMode(DIRO, OUTPUT);
  digitalWrite(DIRO, HIGH);
  Serial.println("SDI-12 ready");
  // Sets up the screen by initalising, turning the screen landscape and clearing it
  display.initR(INITR_BLACKTAB);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);
}

void loop()
{
  handleButtons();

  while (Serial1.available())
  {
    int receivedByte = Serial1.read();

    if (receivedByte == '!')
    {
      SDI12Receive(command);
      command = "";
    }
    else if (receivedByte != 0 && receivedByte != '\r' && receivedByte != '\n')
    {
      command += static_cast<char>(receivedByte);
    }
  }
  unsigned long currentTime = millis();
  if (currentTime - previousDisplayUpdate >= displayInterval)
  {
    previousDisplayUpdate = currentTime;
    readSensors();
    displayDashboard();
  }
}
