#pragma once

#include <Arduino.h>

void SDI12Receive(String input);
void SDI12Send(String message);
void handleSDI12Send();
void handleSDI12Measurement();
bool SDI12IsBusy();
bool SDI12MasterIsBusy();
void DiscardSDI12Input();
String combinedMeasurementResponse();
void InitialiseSDI12Master();
void HandleSDI12Master();
void handleSDI12Receive();
