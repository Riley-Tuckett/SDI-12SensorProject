#include "sdi12.h"

#include <Arduino.h>

#include "app_state.h"
#include "config.h"
#include "sensors.h"

namespace
{
  constexpr size_t Sdi12RXBufferSize = 16;
  constexpr unsigned long SDI12RXTimeoutTime = 1000;

  char rxBuffer[Sdi12RXBufferSize];
  size_t rxLength = 0;
  unsigned long lastRxByte = 0;
  bool rxOverflow = false;

  void resetSdi12Parser()
  {
    rxLength = 0;
    rxOverflow = false;
    rxBuffer[0] = '\0';
  }

  void pollSdi12Receive()
  {
    if (SDI12IsBusy())
    {
      resetSdi12Parser();
      return;
    }
    // Do not consume bytes belonging to the external weather-station master.
    if (SDI12MasterIsBusy())
    {
      resetSdi12Parser();
      return;
    }

    // Discard an incomplete command after a timeout.
    if ((rxLength > 0 || rxOverflow) &&
        millis() - lastRxByte >= SDI12RXTimeoutTime)
    {
      Serial.println("SDI-12 RX timeout: discarding incomplete command");
      resetSdi12Parser();
    }

    while (Serial1.available() > 0)
    {
      char c = static_cast<char>(Serial1.read());
      lastRxByte = millis();

      // Ignore line endings.
      if (c == '\r' || c == '\n')
      {
        continue;
      }

      // UART framing errors can surface as NUL or other control bytes. Discard
      // the rest of that frame so noise cannot become a valid command suffix.
      if (static_cast<uint8_t>(c) < 0x20 || static_cast<uint8_t>(c) > 0x7e)
      {
        rxOverflow = true;
        continue;
      }

      // Discard an overlength command until its terminator.
      if (rxOverflow)
      {
        if (c == '!')
        {
          Serial.println("SDI-12 RX error: command too long");
          resetSdi12Parser();
        }
        continue;
      }

      // Dispatch a complete command without its terminating '!'.
      if (c == '!')
      {
        if (rxLength == 0)
        {
          Serial.println("SDI-12 RX error: empty command");
          resetSdi12Parser();
          continue;
        }

        rxBuffer[rxLength] = '\0';
        String completeCommand(rxBuffer);

        resetSdi12Parser();
        SDI12Receive(completeCommand);
        continue;
      }

      if (rxLength >= Sdi12RXBufferSize - 1)
      {
        rxOverflow = true;
        continue;
      }

      rxBuffer[rxLength++] = c;
      rxBuffer[rxLength] = '\0';
    }
  }


  enum class SendState
  {
    Idle,
    Wait5Ms,
    Sending,
    Wait10Ms
  };

  SendState sendState = SendState::Idle;
  unsigned long stateStartTime = 0;
  bool measurementPending = false;
  bool slaveMeasurementActive = false;
  unsigned long measurementStartTime = 0;
  unsigned long measurementDurationMs = 2000;
  unsigned long slaveMeasurementDeadline = 0;
  constexpr unsigned long bmeMeasurementDurationMs = 2000;
  constexpr unsigned long bhMeasurementDurationMs = 1000;

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

  String dataResponseSegment(const String &response, uint8_t segmentIndex)
  {
    constexpr size_t maxResponseLength = 35;
    String segment = String(deviceAddress);
    uint8_t currentSegment = 0;

    // Each signed value starts with '+' or '-'. Keep values intact while
    // splitting the payload across the standard D0-D9 response slots.
    for (unsigned int start = 1; start < response.length();)
    {
      unsigned int end = start + 1;
      while (end < response.length() && response.charAt(end) != '+' && response.charAt(end) != '-')
      {
        end++;
      }

      String value = response.substring(start, end);
      if (segment.length() + value.length() > maxResponseLength)
      {
        if (currentSegment == segmentIndex)
        {
          return segment;
        }
        currentSegment++;
        segment = String(deviceAddress);
      }
      segment += value;
      start = end;
    }

    return currentSegment == segmentIndex ? segment : String();
  }

  unsigned long localMeasurementDurationMs()
  {
    unsigned long duration = 0;
    if (bmeExists && bmeMeasurementDurationMs > duration)
    {
      duration = bmeMeasurementDurationMs;
    }
    if (bhExists && bhMeasurementDurationMs > duration)
    {
      duration = bhMeasurementDurationMs;
    }
    return duration;
  }

  void discardIncomingBytes()
  {
    while (Serial1.available())
    {
      Serial1.read();
    }
    command = "";
  }

  uint8_t countSignedValues(const String &values)
  {
    uint8_t count = 0;
    for (unsigned int index = 0; index < values.length(); index++)
    {
      if (values.charAt(index) == '+' || values.charAt(index) == '-')
      {
        count++;
      }
    }
    return count;
  }
}



void SDI12Send(String message)
{
  Serial.print("message: ");
  Serial.println(message);
  if (sendState != SendState::Idle)
  {
    return;
  }

  discardIncomingBytes();
  sdi12PendingMessage = message;
  sdi12Active = true;
  digitalWrite(DIRO, LOW);
  stateStartTime = millis();
  sendState = SendState::Wait5Ms;
}
void handleSDI12Receive(){
  pollSdi12Receive();
}

void handleSDI12Send()
{
  if (sendState == SendState::Wait5Ms && millis() - stateStartTime >= 5)
  {
    Serial1.print(sdi12PendingMessage);
    Serial1.print("\r\n");
    stateStartTime = millis();
    sendState = SendState::Sending;
  }

  if (sendState == SendState::Sending)
  {
    unsigned long transmissionTime = (sdi12PendingMessage.length() + 2) * 9;
    if (millis() - stateStartTime >= transmissionTime)
    {
      stateStartTime = millis();
      sendState = SendState::Wait10Ms;
    }
  }

  if (sendState == SendState::Wait10Ms && millis() - stateStartTime >= 10)
  {
    digitalWrite(DIRO, HIGH);
    sendState = SendState::Idle;
    sdi12Active = false;
    discardIncomingBytes();
  }
}

bool SDI12IsBusy()
{
  return sendState != SendState::Idle || sdi12MasterBusy;
}

bool SDI12SlaveMeasurementActive()
{
  return slaveMeasurementActive;
}

void DiscardSDI12Input()
{
  discardIncomingBytes();
}

void handleSDI12Measurement()
{
  // Sensor samples arrive continuously from the timer path. This handler only
  // announces that the SDI-12 measurement window has elapsed.
  if (measurementPending && millis() - measurementStartTime >= measurementDurationMs)
  {
    measurementReady = countSignedValues(combinedMeasurementResponse().substring(1)) > 0;
    measurementPending = false;
  }

  if (slaveMeasurementActive &&
      static_cast<long>(millis() - slaveMeasurementDeadline) >= 0)
  {
    slaveMeasurementActive = false;
  }
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
  if (input.length() < 2)
  {
    errorMessage();
    return;
  }
  if (input.charAt(0) != deviceAddress)
  {
    // On a shared SDI-12 bus, commands for another sensor address are ignored.
    return;
  }

  char commandType = input.charAt(1);
  switch (commandType)
  {
  case 'A':
    if (input.length() != 3)
    {
      errorMessage();
      return;
    }
    {
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
    }
    return;

  case 'M':
    if (input.length() != 2)
    {
      errorMessage();
      return;
    }
    measurementReady = false;
    measurementPending = true;
    measurementDurationMs = localMeasurementDurationMs();
    measurementStartTime = millis();
    slaveMeasurementActive = true;
    slaveMeasurementDeadline = measurementStartTime + measurementDurationMs + 10000;
    {
      String responseValues = combinedMeasurementResponse();
      uint8_t activeValueCount = countSignedValues(responseValues.substring(1));
      unsigned long completionSeconds = (measurementDurationMs + 999) / 1000;
      String response = String(deviceAddress);
      if (completionSeconds < 100)
      {
        response += "0";
      }
      if (completionSeconds < 10)
      {
        response += "0";
      }
      response += String(completionSeconds);
      uint8_t reportCount = (activeValueCount > 9) ? 9 : activeValueCount;
      response += String(reportCount);
      SDI12Send(response);
    }
    return;

  case 'D':
    if (input.length() != 3 || input.charAt(2) < '0' || input.charAt(2) > '9')
    {
      errorMessage();
      return;
    }
    if (measurementPending || !measurementReady)
    {
      errorMessage();
      return;
    }

    {
      String response = dataResponseSegment(combinedMeasurementResponse(), input.charAt(2) - '0');
      if (response.length() <= 1)
      {
        errorMessage();
        return;
      }
      slaveMeasurementActive = false;
      SDI12Send(response);
      return;
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
