#include <Arduino.h>
#include <Wire.h>

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
    initializeSdCard();
    initializeSensors();

    initializeSensorSamplingTimer();
    Serial1.begin(1200, SERIAL_7E1);
    pinMode(DIRO, OUTPUT);
    digitalWrite(DIRO, HIGH);

    initializeDisplay();
    initializeLedTimer();
    configureLogTimer();
  }

  void receiveSdi12Commands()
  {
    if (sdi12IsBusy())
    {
      discardSdi12Input();
      return;
    }

    while (Serial1.available())
    {
      int receivedByte = Serial1.read();
      if (receivedByte == '!')
      {
        sdi12Receive(command);
        command = "";
      }
      else if (receivedByte != 0 && receivedByte != '\r' && receivedByte != '\n')
      {
        command += static_cast<char>(receivedByte);
      }
    }
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
      writeDataToSd();
    }
  }
}

void setup()
{
  Serial.begin(9600);
  initializeHardware();
  Serial.println("SDI-12 ready");
  Serial.println("Hardware timer logging started: 30-second interval");
}

void loop()
{
  handleSdi12Send();
  handleSdi12Measurement();
  receiveSdi12Commands();

  handleButton1Interrupt();

  if (!sdi12IsBusy() && !Serial1.available() && isTimerForSensorSamplingTriggered)
  {
    noInterrupts();
    isTimerForSensorSamplingTriggered = false;
    interrupts();
    averageSensorSample();
  }

  handleButtons();

  if (!sdi12IsBusy() && !Serial1.available())
  {
    processLogRequests();
  }

  unsigned long currentTime = millis();
  if (!sdi12IsBusy() && !Serial1.available() &&
      currentTime - previousDisplayUpdate >= displayInterval)
  {
    previousDisplayUpdate = currentTime;
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
