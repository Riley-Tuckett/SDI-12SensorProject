// INCLUDES:
#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <BH1750.h>
#include <Wire.h>
#include <RTClib.h>
#include "SdFat.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// DEFINITIONS:
#define DIRO 8
#define TFT_CS 10
#define TFT_RST 6
#define TFT_DC 7
#define TFT_SCLK 13
#define TFT_MOSI 11

// Button Configs:
const int button1Pin = 2;
const int button2Pin = 3;
const int alertLEDPin = 4;

const float lightAlertThresholdLux = 100.0F;
const float humidityAlertThresholdPercent = 80.0F;
const float gasAlertThresholdKohm = 100.0F;

const unsigned long debounceMS = 50;

// Time variables
int hours = 0;
int minutes = 0;
int seconds = 0;

// Hardware timer logging configuration (Nathan-Branch timer for 30s logs)
volatile bool logRequested = false;
volatile uint8_t halfSecondTicks = 0;

const uint32_t TIMER_RC = 41016UL; // Approximately 0.5 seconds
const uint8_t TICKS_PER_LOG = 60;   // 60 x 0.5 seconds = 30 seconds

void configureLogTimer()
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC5);

  // TC1 channel 2 is TC5.
  // TIMER_CLOCK5 = 84 MHz / 1024 = approximately 82,031 Hz.
  TC_Configure(TC1, 2, TC_CMR_TCCLKS_TIMER_CLOCK5 | TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC);

  TC_SetRC(TC1, 2, TIMER_RC);

  TC1->TC_CHANNEL[2].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[2].TC_IDR = ~TC_IER_CPCS;

  NVIC_ClearPendingIRQ(TC5_IRQn);
  NVIC_EnableIRQ(TC5_IRQn);

  TC_Start(TC1, 2);
}

// Interrupt handler for TC5 (Logging interval)
void TC5_Handler()
{
  TC_GetStatus(TC1, 2); // Clear the timer interrupt status

  halfSecondTicks++;

  if (halfSecondTicks >= TICKS_PER_LOG)
  {
    halfSecondTicks = 0;
    logRequested = true;
  }
}

volatile bool manualLogRequested = false;
volatile bool button1EdgePending = false;
volatile bool button1InterruptLocked = false;

bool button1DebounceActive = false;
unsigned long button1DebounceStart = 0;

void button1ISR()
{
  if (!button1InterruptLocked)
  {
    button1EdgePending = true;
    button1InterruptLocked = true;
  }
}

// Button 2 debounce state
int button2LastReading = HIGH;
int button2StableState = HIGH;
unsigned long button2LastChange = 0;

// For current dashboard screen state
bool showSystemInfo = false; // RUBY USE THIS VARIABLE PLS

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
bool environmentalAlert = false;
float ambientLightLux = 0.0F;
float humidityPercent = 0.0F;
float gasResistanceKOhm = 0.0F;
unsigned long previousDisplayUpdate = 0;
const unsigned long displayInterval = 1000;
unsigned long previousButtonDebug = 0;

// SD CARD SETUP:
SdFs sd;
FsFile dataLOG;
const uint8_t SD_CS_PIN = A3;
const uint8_t SOFT_MISO_PIN = 12;
const uint8_t SOFT_MOSI_PIN = 11;
const uint8_t SOFT_SCK_PIN = 13;

SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;

// SPECIAL SETUP:
RTC_DS1307 rtc;
#define SDConfig SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(4), &softSpi)
bool sdReady = false;

// SENSOR SAMPLING INTERRUPT && AVERAGE SAMPLING (Main branch):
volatile bool isTimerForSensorSamplingTriggered = false; // 50 ms timer flag for TC4.
class AverageSamplingClass {
  public:
    float tempSamples[10];
    float humiditySamples[10];
    float pressureSamples[10];
    float gasSamples[10];
    float luxSamples[10];
    int bmeIndex = 0;
    int bmeSampleCount = 0;
    int luxIndex = 0;
    int luxSampleCount = 0;
  AverageSamplingClass() : 
      tempSamples{0}, 
      humiditySamples{0}, 
      pressureSamples{0}, 
      gasSamples{0}, 
      luxSamples{0},
      bmeIndex(0),
      bmeSampleCount(0),
      luxIndex(0),
      luxSampleCount(0)
    {}
};
AverageSamplingClass sensorSampler;
float averageSampleValues[5];
bool bmeValid = false;
bool bhValid = false;

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
  environmentalAlert = false;
  bmeValid = false;
  bhValid = false;

  if (bmeExists && bme.performReading())
  {
    humidityPercent = bme.humidity;
    gasResistanceKOhm = bme.gas_resistance / 1000.0F;
    measurements[measurementCount++] = String(bme.temperature, 1);
    measurements[measurementCount++] = String(bme.humidity, 1);
    measurements[measurementCount++] = String(bme.pressure / 100.0F, 1);
    measurements[measurementCount++] = String(bme.gas_resistance / 1000.0F, 1);
    
    bmeValid = true;
  }

  if (bhExists)
  {
    ambientLightLux = lightMeter.readLightLevel();
    measurements[measurementCount++] = String(ambientLightLux, 1);

    bhValid = true;
  }

  measurementReady = measurementCount > 0;
  environmentalAlert = (bhExists && ambientLightLux < lightAlertThresholdLux) ||
                       (bmeExists && humidityPercent > humidityAlertThresholdPercent) ||
                       (bmeExists && gasResistanceKOhm > gasAlertThresholdKohm);
  digitalWrite(alertLEDPin, environmentalAlert ? HIGH : LOW);
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

  case 'M':
  {
    if (input.length() != 2)
    {
      errorMessage();
      return;
    }

    initializeSensors();
    readSensors();
    String response = String(deviceAddress) + "002";
    if (measurementCount < 10)
    {
      response += "0";
    }
    response += String(measurementCount);
    SDI12Send(response);
    return;
  }

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

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  sdReady = sd.begin(SDConfig);
  if (!sdReady)
  {
    Serial.println("SD card initialization failed! Check wiring or card format (FAT32).");
    return;
  }

  Serial.println("SD card initialized successfully!");

  dataLOG = sd.open("LOG.CSV", O_RDWR | O_CREAT | O_APPEND);
  if (dataLOG == true)
  {
    if (dataLOG.size() == 0)
    {
      dataLOG.println(F("Tempurature,Humidity,Pressure,GasResistance,Lux,hourRTC,minuteRTC,secondRTC"));
      dataLOG.sync();
    }
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

// Button debouncing logic
bool buttonPressed(int pin, int &lastReading, int &stableState, unsigned long &lastChange)
{
  int reading = digitalRead(pin);
  bool pressed = (lastReading == HIGH && reading == LOW);

  if (reading != lastReading)
  {
    lastReading = reading;
    lastChange = millis();
  }

  if (millis() - lastChange >= debounceMS)
  {
    if (reading != stableState)
    {
      stableState = reading;

      if (stableState == LOW)
      {
        return true;
      }
    }
  }

  lastReading = reading;
  stableState = reading;
  lastChange = millis();

  return pressed;
}

// Handle buttons
void handleButtons()
{
  // Button 2: To toggle dashboard screen
  if (buttonPressed(button2Pin, button2LastReading, button2StableState, button2LastChange))
  {
    showSystemInfo = !showSystemInfo;

    if (showSystemInfo)
    {
      Serial.println("Screen: System info");
    }
    else
    {
      Serial.println("Screen: LIVE DATA");
    }
  }
}

void InitialiseHardwareTimerForSensorSampling() {
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk((uint32_t)TC4_IRQn);
  
  TC_Configure(TC1, 1, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK4);
  
  uint32_t rc = VARIANT_MCK / 128 / 20; // 20 Hz = 50 ms interval.
  TC_SetRC(TC1, 1, rc);
  
  TC1->TC_CHANNEL[1].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[1].TC_IDR = ~TC_IER_CPCS;
  
  NVIC_EnableIRQ(TC4_IRQn);
  TC_Start(TC1, 1);
}

void TC4_Handler() {
  TC_GetStatus(TC1, 1);
  isTimerForSensorSamplingTriggered = true;
}

void AverageSensorSample() {
  readSensors();
  if (bmeExists && bmeValid) {
    sensorSampler.tempSamples[sensorSampler.bmeIndex] = measurements[0].toFloat();
    sensorSampler.humiditySamples[sensorSampler.bmeIndex] = measurements[1].toFloat();
    sensorSampler.pressureSamples[sensorSampler.bmeIndex] = measurements[2].toFloat();
    sensorSampler.gasSamples[sensorSampler.bmeIndex] = measurements[3].toFloat();

    sensorSampler.bmeIndex = (sensorSampler.bmeIndex + 1) % 10;
    
    if (sensorSampler.bmeSampleCount < 10) {
      sensorSampler.bmeSampleCount++;
    }

    float sumTemp = 0, sumHum = 0, sumPres = 0, sumGas = 0;
    for (int i = 0; i < sensorSampler.bmeSampleCount; i++) {
      sumTemp += sensorSampler.tempSamples[i];
      sumHum += sensorSampler.humiditySamples[i];
      sumPres += sensorSampler.pressureSamples[i];
      sumGas += sensorSampler.gasSamples[i];
    }

    if (sensorSampler.bmeSampleCount > 0) {
      averageSampleValues[0] = sumTemp / sensorSampler.bmeSampleCount;
      averageSampleValues[1] = sumHum / sensorSampler.bmeSampleCount;
      averageSampleValues[2] = sumPres / sensorSampler.bmeSampleCount;
      averageSampleValues[3] = sumGas / sensorSampler.bmeSampleCount;
    }
  }

  if (bhExists && bhValid) {
    sensorSampler.luxSamples[sensorSampler.luxIndex] = measurements[4].toFloat();

    sensorSampler.luxIndex = (sensorSampler.luxIndex + 1) % 10;
    
    if (sensorSampler.luxSampleCount < 10) {
      sensorSampler.luxSampleCount++;
    }

    float sumLux = 0;
    for (int i = 0; i < sensorSampler.luxSampleCount; i++) {
      sumLux += sensorSampler.luxSamples[i];
    }

    if (sensorSampler.luxSampleCount > 0) {
      averageSampleValues[4] = sumLux / sensorSampler.luxSampleCount;
    }
  }
}

void handleButton1Interrupt()
{
  bool edgeDetected = false;

  noInterrupts();
  if (button1EdgePending){
    button1EdgePending = false;
    edgeDetected = true;
  }
  interrupts();

  if (edgeDetected){
    button1DebounceActive = true;
    button1DebounceStart = millis();
  }

  if (button1DebounceActive && millis() - button1DebounceStart >= debounceMS)
  {
    if (digitalRead(button1Pin) == LOW)
    {
      manualLogRequested = true;
      Serial.println("Button 1 pressed: manual log requested.");
    }
    button1DebounceActive = false;
  }

  if (digitalRead(button1Pin) == HIGH){
    noInterrupts();
    button1InterruptLocked = false;
    interrupts();
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(alertLEDPin, OUTPUT);
  digitalWrite(alertLEDPin, LOW);
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(button1Pin), button1ISR, FALLING);
  
  Wire.begin();
  scanI2C();

  rtc.begin();
  rtc.adjust(DateTime(2026, 9, 18, 11, 59, 50));

  InitialiseSDCard();
  initializeSensors();

  InitialiseHardwareTimerForSensorSampling();

  Serial1.begin(1200, SERIAL_7E1);
  pinMode(DIRO, OUTPUT);
  digitalWrite(DIRO, HIGH);
  Serial.println("SDI-12 ready");
  
  display.initR(INITR_BLACKTAB);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);

  configureLogTimer();
  Serial.println("Hardware timer logging started: 30-second interval");
}

void loop()
{
  handleButton1Interrupt();

  if (isTimerForSensorSamplingTriggered) {
    noInterrupts();
    isTimerForSensorSamplingTriggered = false;
    interrupts();

    AverageSensorSample();
  }

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

  bool scheduledLog = false;
  bool manualLog = false;
  noInterrupts();
  manualLog = manualLogRequested;
  manualLogRequested = false;
  if (logRequested){
    logRequested = false;
    scheduledLog = true;
  }
  interrupts();

  if (scheduledLog || manualLog){
    WriteDataToSD();
  }

  unsigned long currentTime = millis();
  if (currentTime - previousDisplayUpdate >= displayInterval)
  {
    previousDisplayUpdate = currentTime;
    readSensors();

    if (showSystemInfo)
    {
      displaySystemInfo();
    }
    else
    {
      displayDashboard();
    }
  }
}