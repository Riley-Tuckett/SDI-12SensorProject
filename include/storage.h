#pragma once

#include <Arduino.h>

extern bool sdReady;

void InitialiseSDCard();
void WriteDataToSD();
String ReadDataSD();
int countLogs();
void refreshLogCount();
uint32_t freeStorageMegabytes();
void refreshFreeStorage();
void ReadRTC();
