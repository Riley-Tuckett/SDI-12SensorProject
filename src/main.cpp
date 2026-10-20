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

// Button Configs
const int BUTTON1_PIN = 2;
const int BUTTON2_PIN = 3;
const int ALERT_LED_PIN = 4;

const float LIGHT_ALERT_THRESHOLD_LUX = 100.0F;
const float HUMIDITY_ALERT_THRESHOLD_PERCENT = 80.0F;
const float GAS_ALERT_THRESHOLD_KOHM = 100.0F;

const unsigned long DEBOUNCE_MS = 50;

//Time stuff i accidentally deleted
int hours = 0;
int minutes = 0;
int seconds = 0;
// For button1 debounce - old polling logic
// int button1LastReading = HIGH;
// int button1StableState = HIGH;
// unsigned long button1LastChange = 0;
// Hardware timer logging configuration
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
  TC_Configure(TC1,2,TC_CMR_TCCLKS_TIMER_CLOCK5 | TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC);

  // 41,016 timer counts at approximately 82,031 counts/second
  // produces an interrupt approximately every 0.5 seconds.
  TC_SetRC(TC1, 2, TIMER_RC);

  TC1->TC_CHANNEL[2].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[2].TC_IDR = ~TC_IER_CPCS;

  NVIC_ClearPendingIRQ(TC5_IRQn);
  NVIC_EnableIRQ(TC5_IRQn);

  TC_Start(TC1, 2);
}

// Interrupt handler for TC5
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
//long unsigned millisBefore = 0;
//const long interval = 30000;

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

  if (bmeExists && bme.performReading())
  {
    humidityPercent = bme.humidity;
    gasResistanceKOhm = bme.gas_resistance / 1000.0F;
    measurements[measurementCount++] = String(bme.temperature, 1);
    measurements[measurementCount++] = String(bme.humidity, 1);
    measurements[measurementCount++] = String(bme.pressure / 100.0F, 1);
    measurements[measurementCount++] = String(bme.gas_resistance / 1000.0F, 1);
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

// Button debouncing logic
bool buttonPressed(int pin, int &lastReading, int &stableState, unsigned long &lastChange)
{
  int reading = digitalRead(pin);

    if (reading != lastReading)
    {
      lastReading = reading;
      lastChange = millis();
    }

    if (millis() - lastChange >= DEBOUNCE_MS &&
        reading != stableState)
    {
      stableState = reading;

      if (stableState == LOW)
      {
        return true;
      }
    }

    return false;
}

// Handle da buttons
void handleButtons()
{

  // Button 1: Manual logging - Old polling stuff see ISR above
  //  if (buttonPressed(BUTTON1_PIN, button1LastReading, button1StableState, button1LastChange))
  // {
  //   manualLogRequested = true;
  //   Serial.println("Button 1 pressed: Manual log reuested");
  // }

  // Button 2: To toggle dashboard screen
  if (buttonPressed(BUTTON2_PIN, button2LastReading, button2StableState, button2LastChange))
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

void handleButton1Interrupt()
{
  bool edgeDetected = false;

  // Safely retrieve the event from the ISR.
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

  // Wait for the button state to settle without blocking.
  if (button1DebounceActive && millis() - button1DebounceStart >= DEBOUNCE_MS)
  {
    if (digitalRead(BUTTON1_PIN) == LOW)
    {
      manualLogRequested = true;
      Serial.println("Button 1 pressed: manual log requested.");
    }
    button1DebounceActive = false;
  }

  // Re-arm the interrupt after the button is released.
  if (digitalRead(BUTTON1_PIN) == HIGH){
    noInterrupts();
    button1InterruptLocked = false;
    interrupts();
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(ALERT_LED_PIN, OUTPUT);
  digitalWrite(ALERT_LED_PIN, LOW);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN),button1ISR,FALLING);
  Wire.begin();
  scanI2C();

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

  //Setting up da timer for data logging (periodically)
  configureLogTimer();
  Serial.println("Hardware timer logging started: 30-second interval");

}

void loop()
{
  handleButton1Interrupt();
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

  // Old timer code
  // if (millis() - millisBefore >= interval)
  // {
  //   millisBefore = millis();
  //   WriteDataToSD();
  // }
  // New timer code
  bool scheduledLog = false;
  bool manualLog = false;
  noInterrupts(); //clears flags makes sure things don't break
  manualLog = manualLogRequested;
  manualLogRequested = false;
  if (logRequested){
    logRequested = false;
    scheduledLog = true;
  }
  interrupts();

  if(scheduledLog || manualLog){
    WriteDataToSD();
  }

  unsigned long currentTime = millis();
  if (currentTime - previousDisplayUpdate >= displayInterval)
  {
    previousDisplayUpdate = currentTime;
    readSensors();
    if (showSystemInfo){
      displaySystemInfo();
    }
    else{
      displayDashboard();
    }
  }
}