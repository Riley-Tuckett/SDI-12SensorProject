#pragma once

#include <Arduino.h>

void sdi12Receive(String input);
void sdi12Send(String message);
void handleSdi12Send();
void handleSdi12Measurement();
bool sdi12IsBusy();
void discardSdi12Input();
