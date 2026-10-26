#include "buttons.h"

#include <Arduino.h>

#include "app_state.h"
#include "config.h"

namespace
{
  void button2Isr()
  {
    if (digitalRead(button2Pin) == LOW)
    {
      if (!button2InterruptLocked)
      {
        button2EdgePending = true;
        button2InterruptLocked = true;
      }
    }
    else
    {
      button2InterruptLocked = false;
    }
  }

  void button1Isr()
  {
    if (digitalRead(button1Pin) == LOW)
    {
      if (!button1InterruptLocked)
      {
        button1EdgePending = true;
        button1InterruptLocked = true;
      }
    }
    else
    {
      button1InterruptLocked = false;
    }
  }
}

void initializeButtons()
{
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  button1InterruptLocked = digitalRead(button1Pin) == LOW;
  button2InterruptLocked = digitalRead(button2Pin) == LOW;
  button1EdgePending = false;
  button2EdgePending = false;
  attachInterrupt(digitalPinToInterrupt(button1Pin), button1Isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(button2Pin), button2Isr, CHANGE);

  noInterrupts();
  button1EdgePending = false;
  button2EdgePending = false;
  button1InterruptLocked = digitalRead(button1Pin) == LOW;
  button2InterruptLocked = digitalRead(button2Pin) == LOW;
  interrupts();
}

void handleButton1Interrupt()
{
  bool edgeDetected = false;
  noInterrupts();
  if (button1EdgePending)
  {
    button1EdgePending = false;
    edgeDetected = true;
  }
  interrupts();

  if (edgeDetected)
  {
    manualLogRequested = true;
    Serial.println("Button 1 pressed: manual log requested.");
  }
}

void handleButtons()
{
  bool edgeDetected = false;
  noInterrupts();
  if (button2EdgePending)
  {
    button2EdgePending = false;
    edgeDetected = true;
  }
  interrupts();

  if (edgeDetected)
  {
    showSystemInfo = !showSystemInfo;
    Serial.println(showSystemInfo ? "Screen: System info" : "Screen: LIVE DATA");
  }
}
