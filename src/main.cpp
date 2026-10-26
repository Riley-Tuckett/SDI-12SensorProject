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
    InitialiseSDCard();
    initializeSensors();

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
      WriteDataToSD();
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
    processLogRequests();
  }

  unsigned long currentTime = millis();
  if (!SDI12IsBusy() && !Serial1.available() &&
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