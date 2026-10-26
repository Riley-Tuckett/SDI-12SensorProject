#include <Arduino.h>
#include <Wire.h>

// Button Configs
const int BUTTON1_PIN = 2;
const int BUTTON2_PIN = 3;
const int ALERT_LED_PIN = 4;

const float LIGHT_ALERT_THRESHOLD_LUX = 100.0F;
const float HUMIDITY_ALERT_THRESHOLD_PERCENT = 80.0F;
const float GAS_ALERT_THRESHOLD_KOHM = 100.0F;

const unsigned long DEBOUNCE_MS = 50;

const int SDI_12_STATUS_LED_PIN = 9;

// For button1 debounce
int button1LastReading = HIGH;
int button1StableState = HIGH;
unsigned long button1LastChange = 0;

// Button 2 debounce state
int button2LastReading = HIGH;
int button2StableState = HIGH;
unsigned long button2LastChange = 0;

// For current dashboard screen state
int currentScreen = 0;

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
String sdi12PendingMessage = "";
uint8_t measurementCount = 0;
bool bmeExists = false;
bool bhExists = false;
bool measurementReady = false;
bool environmentalAlert = false;
volatile bool sdi12Active = false;
volatile bool sdi12State = false;
volatile int sdi12BlinkCount = 0;
float ambientLightLux = 0.0F;
float humidityPercent = 0.0F;
float gasResistanceKOhm = 0.0F;
int temperatureRisk = 0;
int humidityRisk = 0;
int pressureRisk = 0;
int environmentalRisk = 0;
const int buzzerPin = 5;
unsigned long previousDisplayUpdate = 0;
const unsigned long displayInterval = 1000;
unsigned long previousButtonDebug = 0;
unsigned long sdi12StateStartTime = 0;
enum SDI12SendState
{
  SDI_IDLE,
  SDI_WAIT_5MS,
  SDI_SENDING,
  SDI_WAIT_10MS
};
SDI12SendState sdi12SendState = SDI_IDLE;

// SD CARD SETUP:
SdFs sd;
FsFile dataLOG;
// Chip select may be constant or RAM variable.
const uint8_t SD_CS_PIN = A3;
// Pin numbers in templates must be constants.
const uint8_t SOFT_MISO_PIN = 12;
const uint8_t SOFT_MOSI_PIN = 11;
const uint8_t SOFT_SCK_PIN = 13;

// SdFat software SPI template
SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;

// SPECIAL SETUP:
RTC_DS1307 rtc;
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(4), &softSpi)
bool sdReady = false;

// TIME:
long unsigned millisBefore = 0;
const long interval = 30000;
int hours = 0;
int minutes = 0;
int seconds = 0;

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

// This is a very cool error message :).
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

  if (sdi12SendState != SDI_IDLE)
  {
    return;
  }

  sdi12PendingMessage = message;

  sdi12Active = true;
  digitalWrite(DIRO, LOW);
  sdi12StateStartTime = millis();
  sdi12SendState = SDI_WAIT_5MS;
  return;
}

void handleSDI12Send()
{
  // First state
  if (sdi12SendState == SDI_WAIT_5MS)
  {
    if (millis() - sdi12StateStartTime >= 5)
    {
      Serial1.print(sdi12PendingMessage);
      Serial1.print("\r\n");
      sdi12StateStartTime = millis();
      sdi12SendState = SDI_SENDING;
    }
  }

  // Second State
  if (sdi12SendState == SDI_SENDING)
  {
    unsigned long transmissionTime = (sdi12PendingMessage.length() + 2) * 9;
    if (millis() - sdi12StateStartTime >= transmissionTime)
    {
      sdi12StateStartTime = millis();
      sdi12SendState = SDI_WAIT_10MS;
    }
  }

  // Third State
  if (sdi12SendState == SDI_WAIT_10MS)
  {
    if (millis() - sdi12StateStartTime >= 10)
    {
      digitalWrite(DIRO, HIGH);
      sdi12SendState = SDI_IDLE;
      sdi12Active = false;
    }
  }
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
  environmentalAlert = false;

  if (bmeExists && bme.performReading())
  {
    humidityPercent = bme.humidity;
    gasResistanceKOhm = bme.gas_resistance / 1000.0F;
    measurements[measurementCount++] = String(bme.temperature, 1);
    measurements[measurementCount++] = String(bme.humidity, 1);
    measurements[measurementCount++] = String(bme.pressure / 100.0F, 1);
    measurements[measurementCount++] = String(bme.gas_resistance / 1000.0F, 1);

    temperatureRisk = calculateTemperatureRisk(bme.temperature);
    humidityRisk = calculateHumidityRisk(bme.humidity);
    pressureRisk = calculatePressureRisk(bme.pressure / 100.0F);

    environmentalRisk = calculateEnvironmentalRisk(temperatureRisk, humidityRisk, pressureRisk);

    if (environmentalRisk >= 67)
    {
      digitalWrite(buzzerPin, HIGH);
    }
    else
    {
      digitalWrite(buzzerPin, LOW);
    }
  }

  if (bhExists)
  {
    ambientLightLux = lightMeter.readLightLevel();
    measurements[measurementCount++] = String(ambientLightLux, 1);
  }

  measurementReady = measurementCount > 0;
  environmentalAlert = (bhExists && ambientLightLux < LIGHT_ALERT_THRESHOLD_LUX) ||
                       (bmeExists && humidityPercent > HUMIDITY_ALERT_THRESHOLD_PERCENT) ||
                       (bmeExists && gasResistanceKOhm > GAS_ALERT_THRESHOLD_KOHM);
  digitalWrite(ALERT_LED_PIN, environmentalAlert ? HIGH : LOW);
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
#include "app_state.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "sdi12.h"
#include "sensors.h"
#include "storage.h"
#include "timers.h"

namespace
{
  void initializeHardware()
  {
    pinMode(alertLEDPin, OUTPUT);
    digitalWrite(alertLEDPin, LOW);
    pinMode(SDI_12_STATUS_LED_PIN, OUTPUT);
    digitalWrite(SDI_12_STATUS_LED_PIN, LOW);
    initializeButtons();

    Wire.begin();
    scanI2C();
    InitialiseSDCard();
    initializeSensors();

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
    InitialiseHardwareTimerForSensorSampling();
    Serial1.begin(1200, SERIAL_7E1);
    pinMode(DIRO, OUTPUT);
    digitalWrite(DIRO, HIGH);
    InitialiseSDI12Master();

    initializeDisplay();
    initializeLedTimer();
    configureLogTimer();
  }

  void receiveSdi12Commands()
  {
    if (SDI12MasterIsBusy())
    {
      return;
    }

    if (SDI12IsBusy())
    {
      DiscardSDI12Input();
      return;
    }

    while (Serial1.available())
    {
      int receivedByte = Serial1.read();
      if (receivedByte == '!')
      {
        SDI12Receive(command);
        command = "";
      }
      else if (receivedByte == '\r' || receivedByte == '\n')
      {
        // Commands never contain carriage return or line feed. Any CR/LF without
        // an exclamation mark represents a sensor response or noise on the bus.
        command = "";
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

void ReadRTC()
{
  DateTime now = rtc.now();
  hours = now.hour();
  minutes = now.minute();
  seconds = now.second();
}

String ReadDataSD()
{
  String fileContent = "";
  FsFile dataLOG = sd.open("LOG.CSV", O_READ);
  if (dataLOG == true)
  {
    while (dataLOG.available())
    {
      char character = dataLOG.read();
      Serial.write(character);
      fileContent += character;
    }
    dataLOG.close();
  }
  return fileContent;
}

void WriteDataToSD()
{
  /*TYPE:
  1. temp
  2. humidity
  3. gas
  4. lux
  5. hour
  6. minute
  7. second
  */
  readSensors();
  ReadRTC();

  if (!sdReady)
  {
    Serial.println("Skipping SD write: SD card is not initialized.");
    return;
  }

  FsFile dataLOG = sd.open("LOG.CSV", O_RDWR | O_CREAT | O_APPEND);
  if (dataLOG == true)
  {
    dataLOG.print(measurements[0]);
    dataLOG.print(',');
    dataLOG.print(measurements[1]);
    dataLOG.print(',');
    dataLOG.print(measurements[2]);
    dataLOG.print(',');
    dataLOG.print(measurements[3]);
    dataLOG.print(',');
    dataLOG.print(measurements[4]);
    dataLOG.print(',');
    dataLOG.print(hours);
    dataLOG.print(',');
    dataLOG.print(minutes);
    dataLOG.print(',');
    dataLOG.println(seconds);
    dataLOG.sync();
    dataLOG.close();
    Serial.println("Successfully wrote to SD card.");
  }
  else
  {
    Serial.println("Failed to open file for writing.");
  }
}

void InitialiseSDCard()
{
  Serial.println("Initializing SD card using Software SPI...");

  // Configure the SD card Chip Select pin.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  // Initialize the SD card using the Software SPI configuration.
  sdReady = sd.begin(SD_CONFIG);
  if (!sdReady)
  {
    Serial.println("SD card initialization failed! Check wiring or card format (FAT32).");
    return;
  }

  Serial.println("SD card initialized successfully!");

  // Open or create "LOG.CSV" in read/write/append mode.
  dataLOG = sd.open("LOG.CSV", O_RDWR | O_CREAT | O_APPEND);
  if (dataLOG == true)
  {
    // If the file is brand new, write a CSV header row.
    if (dataLOG.size() == 0)
    {
      dataLOG.println(F("Tempurature,Humidity,Pressure,GasResistance,Lux,hourRTC,minuteRTC,secondRTC"));
      dataLOG.sync(); // Save the header immediately.
    }
    // Close the file when done writing.
    dataLOG.close();

    Serial.println(F("Test data written to LOG.CSV successfully!"));
  }
  else
  {
    Serial.println(F("Error: Could not open LOG.CSV for writing."));
  }
}

void displayDashboard()
{
  display.fillScreen(ST77XX_BLACK);

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("Sensor Dashboard");

  display.setCursor(5, 15);
  display.setTextColor(environmentalAlert ? ST77XX_RED : ST77XX_GREEN);
  display.println(environmentalAlert ? "[ALERT]" : "[OK]");
  display.setTextColor(ST77XX_WHITE);

  display.setCursor(5, 30);
  display.print("Temp: ");
  display.print(measurements[0]);
  display.println(" C");

  display.setCursor(5, 40);
  display.print("Humidity: ");
  display.print(measurements[1]);
  display.println(" %");

  display.setCursor(5, 50);
  display.print("Pressure: ");
  display.print(measurements[2]);
  display.println(" hPa");

  display.setCursor(5, 60);
  display.print("Gas Resistance: ");
  display.print(measurements[3]);
  display.println(" kOhm");

  display.setCursor(5, 70);
  display.print("Light: ");
  display.print(measurements[4]);
  display.println(" lux");

  display.setCursor(5, 90);
  display.print("Logging: ");
  if (sdReady)
  {
    display.setTextColor(ST77XX_GREEN);
    display.println("ACTIVE");
  }
  else
  {
    display.setTextColor(ST77XX_RED);
    display.println("OFF");
  }

  display.setTextColor(ST77XX_WHITE);
}

int countLogs()
{
  if (!sdReady)
  {
    return 0;
  }

  FsFile file = sd.open("LOG.CSV", O_READ);

  if (!file)
  {
    return 0;
  }

  int lineCount = 0;

  while (file.available())
  {
    char character = file.read();

    if (character == '\n')
    {
      lineCount++;
    }
  }

  file.close();
  return lineCount > 0 ? lineCount - 1 : 0;
}

void displaySystemInfo()
{
  display.fillScreen(ST77XX_BLACK);

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("System Info");

  display.setCursor(5, 25);
  display.print("Total Logs: ");
  display.println(countLogs());

  display.setCursor(5, 40);
  display.print("Storage: ");
  if (sdReady)
  {
    display.setTextColor(ST77XX_GREEN);
    display.println("READY");
  }
  else
  {
    display.setTextColor(ST77XX_RED);
    display.println("ERROR");
  }

  display.setTextColor(ST77XX_WHITE);
}

void displayEnvironmentalRisk()
{
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("Environmental Risk");

  int centerX = 80;
  int centerY = 95;
  int radius = 45;

  uint16_t colour;

  for (int angle = 0; angle <= 180; angle++)
  {
    if (angle <= 60)
    {
      colour = ST77XX_RED;
    }
    else if (angle < 120)
    {
      colour = ST77XX_YELLOW;
    }
    else
    {
      colour = ST77XX_GREEN;
    }
    float radians = angle * PI / 180.0;

    int x = centerX + cos(radians) * radius;
    int y = centerY - sin(radians) * radius;

    display.fillCircle(x, y, 2, colour);
  }

  float needleAngle = 180.0 - (environmentalRisk * 1.8);
  float needleRadians = needleAngle * PI / 180.0;

  int needleX = centerX + cos(needleRadians) * 35;
  int needleY = centerY - sin(needleRadians) * 35;

  display.drawLine(centerX, centerY, needleX, needleY, ST77XX_WHITE);
  display.fillCircle(centerX, centerY, 3, ST77XX_WHITE);

  display.setCursor(55, 103);
  display.setTextColor(ST77XX_WHITE);
  display.print("Risk: ");
  display.print(environmentalRisk);
  display.print("%");

  display.setCursor(60, 115);

  if (environmentalRisk <= 33)
  {
    display.setTextColor(ST77XX_GREEN);
    display.print("SAFE");
  }
  else if (environmentalRisk <= 66)
  {
    display.setTextColor(ST77XX_YELLOW);
    display.print("CAUTION");
  }
  else
  {
    display.setTextColor(ST77XX_RED);
    display.print("DANGER");
  }
}

// Button debouncing logic
bool buttonPressed(int pin, int &lastReading, int &stableState, unsigned long &lastChange)
{
  int reading = digitalRead(pin);
  bool pressed = (lastReading == HIGH && reading == LOW);

  // Detect a change in the physical button signal
  if (reading != lastReading)
  {
    lastReading = reading;
    lastChange = millis();
  }

  // Accept the change once the signal has been stable
  if (millis() - lastChange >= DEBOUNCE_MS)
  {
    if (reading != stableState)
    {
      stableState = reading;

      // INPUT_PULLUP means LOW indicates a press
      if (stableState == LOW)
      else if (receivedByte != 0)
      {
        command += static_cast<char>(receivedByte);
        if (command.length() > 10)
        {
          command = "";
        }
      }
    }
  }

  lastReading = reading;
  stableState = reading;
  lastChange = millis();

  return pressed;
}

// Handle da buttons
void handleButtons()
{

  // Button 1: Manual logging
  if (buttonPressed(BUTTON1_PIN, button1LastReading, button1StableState, button1LastChange))
  {
    Serial.println("Button 1 pressed: Manual log reuested");
  }

  // Button 2: To toggle dashboard screen
  if (buttonPressed(BUTTON2_PIN, button2LastReading, button2StableState, button2LastChange))
  {
    currentScreen++;

    if (currentScreen > 2)
    {
      currentScreen = 0;
    }

    if (currentScreen == 0)
    {
      Serial.println("Screen: LIVE DATA");
    }
    else if (currentScreen == 1)
    {
      Serial.println("Screen: System info");
    }
    else if (currentScreen == 2)
    {
      Serial.println("Screen: Environmental Risk");
    }
  }
}

void ledTimerSetup()
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC6);
  TC_Configure(TC2, 0, TC_CMR_TCCLKS_TIMER_CLOCK4 | TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC);
  TC_SetRC(TC2, 0, 32813);
  TC2->TC_CHANNEL[0].TC_IDR = 0xFFFFFFFF;
  TC2->TC_CHANNEL[0].TC_IER = TC_IER_CPCS;
  NVIC_EnableIRQ(TC6_IRQn);
  TC_Start(TC2, 0);
}
  void processLogRequests()
  {
    bool scheduledLog = false;
    bool manualLog = false;
    noInterrupts();
    manualLog = manualLogRequested;
    manualLogRequested = false;
    scheduledLog = logRequested;
    logRequested = false;
    interrupts();

    if (scheduledLog || manualLog)
    {
      sdi12BlinkCount = 0;
      sdi12State = !sdi12State;
      digitalWrite(SDI_12_STATUS_LED_PIN, sdi12State ? HIGH : LOW);
    }
  }
  else
  {
    sdi12BlinkCount = 0;
    sdi12State = false;
    digitalWrite(SDI_12_STATUS_LED_PIN, LOW);
  }
}

int calculateTemperatureRisk(float temperature)
{
  if (temperature >= 10 && temperature <= 25)
  {
    return 0;
  }
  if (temperature > 25 && temperature < 45)
  {
    return ((temperature - 25) / (20)) * 100;
  }
  if (temperature < 10 && temperature > 0)
  {
    return ((10 - temperature) / 10) * 100;
  }
  if (temperature <= 0 || temperature >= 45)
  {
    return 100;
  }
  return 0;
}

int calculateHumidityRisk(float humidity)
{
  if (humidity >= 30 && humidity <= 60)
  {
    return 0;
  }
  if (humidity > 60 && humidity < 100)
  {
    return ((humidity - 60) / 40) * 100;
  }
  if (humidity < 30 && humidity > 0)
  {
    return ((30 - humidity) / 30) * 100;
      WriteDataToSD();
    }
  }
  if (humidity <= 0 || humidity >= 100)
  {
    return 100;
  }
  return 0;
}

int calculatePressureRisk(float pressure)
{
  if (pressure >= 1000)
  {
    return 0;
  }
  if (pressure <= 960)
  {
    return 100;
  }
  if (pressure > 960 && pressure < 1000)
  {
    return ((1000 - pressure) / 40) * 100;
  }
  return 0;
}

int calculateEnvironmentalRisk(int temperatureRisk, int humidityRisk, int pressureRisk)
{
  return (temperatureRisk + humidityRisk + pressureRisk) / 3;
}

void watchdogSetup()
{
  WDT_Enable(WDT, WDT_MR_WDRSTEN | WDT_MR_WDV(512) | WDT_MR_WDD(512));
}


void setup()
{
  Serial.begin(9600);
  pinMode(ALERT_LED_PIN, OUTPUT);
  digitalWrite(ALERT_LED_PIN, LOW);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  Wire.begin();
  scanI2C();

  pinMode(SDI_12_STATUS_LED_PIN, OUTPUT);
  digitalWrite(SDI_12_STATUS_LED_PIN, LOW);

  // THE RTC BEGINSSSSSSS!!!!!!!!!
  rtc.begin();
  rtc.adjust(DateTime(2026, 9, 18, 11, 59, 50));

  InitialiseSDCard();
  initializeSensors();

  Serial1.begin(1200, SERIAL_7E1);
  pinMode(DIRO, OUTPUT);
  digitalWrite(DIRO, HIGH);
  Serial.println("SDI-12 ready");
  // Sets up the screen by initalising, turning the screen landscape and clearing it
  display.initR(INITR_BLACKTAB);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);

  ledTimerSetup();
  initializeHardware();
  Serial.println("SDI-12 ready");
  Serial.println("Hardware timer logging started: 30-second interval");
}

void loop()
{
  // Keep the SDI-12 bus responsive before doing any slower sensor or SD work.
  handleSDI12Send();
  HandleSDI12Master();
  handleSDI12Measurement();
  handleSDI12Receive();

  handleButton1Interrupt();

  // A timer interrupt only raises the flag; the actual I2C read stays out of the ISR.
  if (!SDI12IsBusy() && !Serial1.available() && isTimerForSensorSamplingTriggered)
  {
    noInterrupts();
    isTimerForSensorSamplingTriggered = false;
    interrupts();
    AverageSensorSample();
  }

  handleButtons();

  if (!SDI12IsBusy() && !Serial1.available())
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
  if (millis() - millisBefore >= interval)
  {
    millisBefore = millis();
    WriteDataToSD();
    processLogRequests();
  }

  unsigned long currentTime = millis();
  if (!SDI12IsBusy() && !Serial1.available() &&
      currentTime - previousDisplayUpdate >= displayInterval)
  {
    previousDisplayUpdate = currentTime;

    if (currentScreen == 0)
    {
      displayDashboard();
    }
    else if (currentScreen == 1)
    if (showSystemInfo)
    {
      displaySystemInfo();
    }
    else if (currentScreen == 2)
    {
      displayEnvironmentalRisk();
    }
  }

  WDT_Restart(WDT);
}
