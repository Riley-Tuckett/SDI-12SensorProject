#include "sdi12.h"

#include <Arduino.h>

#include "app_state.h"
#include "config.h"
#include "sensors.h"

namespace
{
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
  unsigned long measurementStartTime = 0;
  constexpr unsigned long measurementDurationMs = 2000;

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

  String bmeMeasurementResponse()
  {
    return String(deviceAddress) + "+" + String(bme.temperature, 1) +
           "+" + String(bme.humidity, 1) +
           "+" + String(bme.pressure / 100.0F, 1) +
           "+" + String(bme.gas_resistance / 1000.0F, 1);
  }

  String luxMeasurementResponse()
  {
    return String(deviceAddress) + "+" + String(lightMeter.readLightLevel(), 1);
  }

  void discardIncomingBytes()
  {
    while (Serial1.available())
    {
      Serial1.read();
    }
    command = "";
  }
}

void sdi12Send(String message)
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

void handleSdi12Send()
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

bool sdi12IsBusy()
{
  return sendState != SendState::Idle;
}

void discardSdi12Input()
{
  discardIncomingBytes();
}

void handleSdi12Measurement()
{
  if (measurementPending && millis() - measurementStartTime >= measurementDurationMs)
  {
    readSensors();
    measurementPending = false;
  }
}

void sdi12Receive(String input)
{
  Serial.print("Received SDI-12 command: ");
  Serial.println(input);

  if (input == "?")
  {
    sdi12Send(String(deviceAddress));
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
      sdi12Send(String(deviceAddress));
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
    measurementStartTime = millis();
    {
      String response = String(deviceAddress) + "00205";
      sdi12Send(response);
    }
    return;

  case 'D':
    if (input.length() != 3 || input.charAt(2) < '0' || input.charAt(2) > '9')
    {
      errorMessage();
      return;
    }
    switch (input.charAt(2))
    {
    case '0':
      if (measurementPending || !bmeExists || !bme.performReading())
      {
        errorMessage();
        return;
      }
      sdi12Send(bmeMeasurementResponse());
      return;

    case '1':
      if (measurementPending || !bhExists)
      {
        errorMessage();
        return;
      }
      sdi12Send(luxMeasurementResponse());
      return;

    case '2':
      if (measurementPending || !measurementReady)
      {
        errorMessage();
        return;
      }
      sdi12Send(measurementResponse());
      return;

    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return;

    default:
      return;
    }

  case 'I':
    if (input.length() != 2)
    {
      errorMessage();
      return;
    }
    sdi12Send(String(deviceAddress) + deviceIdentification);
    return;

  default:
    errorMessage();
    return;
  }
}
