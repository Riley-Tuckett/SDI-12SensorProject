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

  constexpr uint32_t sdi12BaudRate = 1200;
  // The ATMOS 41 reports a minimum measurement time. Leave a small settling
  // margin before requesting D0 so the request does not land on the completion edge.
  constexpr unsigned long measurementReadyMarginMs = 100;
  constexpr unsigned long dataResponseTimeoutMs = 5000;

  void sendSdi12Break()
  {
    Serial1.flush();
    // Hardware break on USART0: holds Pin 18 (TX) LOW continuously for 13 ms
    // without resetting peripheral registers or changing baud rate.
    // Inverted by the transceiver board into a 5V spacing break (>12 ms) on SDI-12.
    USART0->US_CR = US_CR_STTBRK;
    delay(13);
    USART0->US_CR = US_CR_STPBRK;
    delay(9); // SDI-12 marking period after break

    // The 15 ms spacing break holds RX LOW via loopback, causing a framing error
    // in the SAM3X USART receiver. Drain break-induced NUL/framing bytes and
    // clear the RXRDY/FE/OVRE/PARE status flags so the next real byte is clean.
    while (Serial1.available())
    {
      Serial1.read();
    }
    USART0->US_CR = US_CR_RSTSTA; // Clear framing-error status bits (not RSTRX — keeps Serial1 state intact)

  }

  void startMasterCommand(const String &message, MasterState nextState)
  {
    Serial.print("SDI-12 master TX: ");
    Serial.println(message);

    while (Serial1.available())
    {
      Serial1.read();
    }
    masterInput = "";

    if (message == "0R0!")
    {
      externalWeatherData = "";
      externalWeatherDataReady = false;
    }

    // Follow the working ATMOS 41 reference transaction: break and marking
    // before every command, then send the command terminated by '!'.
    digitalWrite(DIRO, LOW);
    sendSdi12Break();

    Serial1.print(message);
    Serial1.flush();

    // Discard the local converter echo while still driving the bus. The sensor
    // cannot respond until the master releases the line below.
    while (Serial1.available())
    {
      Serial1.read();
    }

    digitalWrite(DIRO, HIGH);
    USART0->US_CR = US_CR_RSTSTA;
    masterInput = "";

    sdi12Active = true;
    sdi12MasterBusy = true;
    masterState = nextState;
    masterDeadline = millis() + (nextState == MasterState::WaitingForData
                                     ? dataResponseTimeoutMs
                                     : 3000UL);
  }

  void finishMasterTransaction(bool dataReceived)
  {
    if (!dataReceived)
    {
      Serial.print("SDI-12 master timeout or invalid response in state ");
      Serial.println(masterState == MasterState::WaitingForData ? "D0 response" : "measurement response");
      if (masterInput.length() > 0)
      {
        Serial.print("SDI-12 partial RX: ");
        Serial.println(masterInput);
      }
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
    Serial.print("SDI-12 master RX: ");
    Serial.println(response);

    if (masterState == MasterState::WaitingForMeasurement)
    {
      // If we received a local command echo, ignore it
      if (response == "0M!" || response == "0M")
      {
        return;
      }

      int addrIndex = response.indexOf('0');
      // Supports standard 5-char ("00039"), 4-char ("0039"), and 3-char ("039")
      if (addrIndex >= 0 && response.length() - addrIndex >= 3)
      {
        String validResponse = response.substring(addrIndex);
        if (validResponse.length() >= 5)
        {
          externalMeasurementDurationMs = validResponse.substring(1, 4).toInt() * 1000UL;
        }
        else if (validResponse.length() == 4)
        {
          externalMeasurementDurationMs = validResponse.substring(1, 3).toInt() * 1000UL;
        }
        else
        {
          // 3-character format: address '0', time (1 digit '3'), count (1 digit '9')
          externalMeasurementDurationMs = validResponse.substring(1, 2).toInt() * 1000UL;
        }

        if (externalMeasurementDurationMs == 0)
        {
          externalMeasurementDurationMs = 3000UL;
        }

        masterDeadline = millis() + externalMeasurementDurationMs + measurementReadyMarginMs;
        masterState = MasterState::WaitingForDataTime;
        Serial.print("Measurement in progress, wait: ");
        Serial.print(externalMeasurementDurationMs);
        Serial.println(" ms");
      }
      else
      {
        finishMasterTransaction(false);
      }
      return;
    }

    if (masterState == MasterState::WaitingForDataTime)
    {
      if (response == "0")
      {
        // Sensor sent service request indicating measurement is ready early.
        // Wait at least 25 ms before issuing 0D0! (SDI-12 requires >= 15 ms after service request).
        masterDeadline = millis() + 25;
      }
      return;
    }

    if (masterState == MasterState::WaitingForData)
    {
      // If we received a local command echo, ignore it
      if (response == "0D0!" || response == "0D0" || response == "0D1!" || response == "0D1" ||
          response == "0D2!" || response == "0D2" || response == "0M!" || response == "0M" ||
          response == "0R0!" || response == "0R0")
      {
        return;
      }

      // Ignore empty lines — these arrive when the trailing '\n' of the command echo
      // is read after masterInput was already cleared, or from bus noise.
      if (response.length() == 0)
      {
        return;
      }

      // Check if response contains sensor address prefix "0+" or "0-"
      int p = response.indexOf("0+");
      if (p < 0)
      {
        p = response.indexOf("0-");
      }

      String dataPayload = "";
      if (p >= 0)
      {
        dataPayload = response.substring(p + 1);
      }
      else
      {
        // If address '0' was omitted or corrupted by an edge glitch, find first '+' or '-'
        int plusIdx = response.indexOf('+');
        int minusIdx = response.indexOf('-');
        int firstSign = -1;
        if (plusIdx >= 0 && minusIdx >= 0)
        {
          firstSign = (plusIdx < minusIdx) ? plusIdx : minusIdx;
        }
        else if (plusIdx >= 0)
        {
          firstSign = plusIdx;
        }
        else if (minusIdx >= 0)
        {
          firstSign = minusIdx;
        }

        if (firstSign >= 0)
        {
          dataPayload = response.substring(firstSign);
        }
      }

      if (dataPayload.length() >= 2 && (dataPayload.charAt(0) == '+' || dataPayload.charAt(0) == '-'))
      {
        externalWeatherData = dataPayload;
        externalWeatherDataReady = true;
        Serial.print("ATM41 R0 values: ");
        Serial.println(externalWeatherData);
        finishMasterTransaction(true);
      }
      else
      {
        finishMasterTransaction(false);
      }
      return;
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
    if (SDI12IsBusy() || SDI12SlaveMeasurementActive() || Serial1.available() ||
        static_cast<long>(millis() - nextMasterQueryTime) < 0)
    {
      return;
    }

    // ATMOS 41 R0 returns its complete weather record in one response.
    startMasterCommand("0R0!", MasterState::WaitingForData);
    return;
  }

  while (Serial1.available())
  {
    char receivedByte = static_cast<char>(Serial1.read());
    if (receivedByte == '\r' || receivedByte == 0)
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
    else if (masterInput.length() < 180)
    {
      masterInput += receivedByte;
    }
  }

  if (masterState == MasterState::WaitingForDataTime &&
      static_cast<long>(millis() - masterDeadline) >= 0)
  {
    startMasterCommand("0D0!", MasterState::WaitingForData);
  }
  else if (masterState != MasterState::WaitingForDataTime &&
           static_cast<long>(millis() - masterDeadline) >= 0)
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
  if (bmeValid && measurementCount >= 2)
  {
    for (uint8_t index = 0; index < 2; index++)
    {
      float value = measurements[index].toFloat();
      if (value >= 0)
      {
        response += "+";
      }
      response += measurements[index];
    }
  }

  if (bhValid)
  {
    uint8_t luxIndex = bmeValid ? 4 : 0;
    if (measurementCount > luxIndex)
    {
      float luxValue = measurements[luxIndex].toFloat();
      if (luxValue >= 0)
      {
        response += "+";
      }
      response += measurements[luxIndex];
    }
  }

  if (externalWeatherDataReady)
  {
    response += externalWeatherData;
  }
  return response;
}
