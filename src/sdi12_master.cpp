#include "sdi12.h"

#include <Arduino.h>

#include "app_state.h"
#include "config.h"
#include "sensors.h"

namespace
{
  enum class MasterState
  {
    Idle,
    WaitingForMeasurement,
    WaitingForDataTime,
    WaitingForData
  };

  // The master and slave share Serial1, so each side must finish its turn before
  // the other side touches the bus.
  MasterState masterState = MasterState::Idle;
  String masterInput;
  unsigned long nextMasterQueryTime = 0;
  unsigned long masterDeadline = 0;
  unsigned long externalMeasurementDurationMs = 2000;

  void startMasterCommand(const String &message, MasterState nextState)
  {
    while (Serial1.available()  )
    {
      Serial1.read();
    }

    masterInput = "";
    digitalWrite(DIRO, LOW);
    Serial1.print(message);
    Serial1.print("\r\n");
    Serial1.flush();
    digitalWrite(DIRO, HIGH);
    sdi12Active = true;
    sdi12MasterBusy = true;
    masterState = nextState;
    masterDeadline = millis() + 3000;
  }

  void finishMasterTransaction(bool dataReceived)
  {
    if (!dataReceived)
    {
      externalWeatherDataReady = false;
    }
    sdi12Active = false;
    sdi12MasterBusy = false;
    masterState = MasterState::Idle;
    masterInput = "";
    nextMasterQueryTime = millis() + masterQueryIntervalMs;
  }

  void processMasterResponse(const String &response)
  {
    if (masterState == MasterState::WaitingForMeasurement)
    {
      if (response.length() < 5 || response.charAt(0) != '0')
      {
        finishMasterTransaction(false);
        return;
      }

      externalMeasurementDurationMs = response.substring(1, 4).toInt() * 1000UL;
      masterDeadline = millis() + externalMeasurementDurationMs;
      masterState = MasterState::WaitingForDataTime;
      return;
    }

    if (masterState == MasterState::WaitingForData)
    {
      if (response.length() > 1 && response.charAt(0) == '0')
      {
        externalWeatherData = response.substring(1);
        externalWeatherDataReady = true;
        finishMasterTransaction(true);
      }
      else
      {
        finishMasterTransaction(false);
      }
    }
  }
}

void InitialiseSDI12Master()
{
  masterState = MasterState::Idle;
  masterInput = "";
  externalWeatherData = "";
  externalWeatherDataReady = false;
  sdi12MasterBusy = false;
  nextMasterQueryTime = millis() + masterInitialDelayMs;
}

void HandleSDI12Master()
{
  if (masterState == MasterState::Idle)
  {
    if (SDI12IsBusy() || Serial1.available() ||
        static_cast<long>(millis() - nextMasterQueryTime) < 0)
    {
      return;
    }

    startMasterCommand("0M!", MasterState::WaitingForMeasurement);
    return;
  }

  while (Serial1.available())
  {
    char receivedByte = static_cast<char>(Serial1.read());
    if (receivedByte == '\r')
    {
      continue;
    }
    if (receivedByte == '\n')
    {
      processMasterResponse(masterInput);
      masterInput = "";
      if (masterState == MasterState::Idle)
      {
        return;
      }
    }
    else if (masterInput.length() < 120)
    {
      masterInput += receivedByte;
    }
  }

  if (masterState == MasterState::WaitingForDataTime &&
      static_cast<long>(millis() - masterDeadline) >= 0)
  {
    startMasterCommand("0D0!", MasterState::WaitingForData);
  }
  else if (static_cast<long>(millis() - masterDeadline) >= 0)
  {
    finishMasterTransaction(false);
  }
}

bool SDI12MasterIsBusy()
{
  return masterState != MasterState::Idle;
}

String combinedMeasurementResponse()
{
  String response = String(deviceAddress);
  const uint8_t localIndexes[] = {0, 1, 4};
  for (uint8_t index : localIndexes)
  {
    if (index < measurementCount)
    {
      float value = measurements[index].toFloat();
      if (value >= 0)
      {
        response += "+";
      }
      response += measurements[index];
    }
  }

  if (externalWeatherDataReady)
  {
    response += externalWeatherData;
  }
  return response;
}
