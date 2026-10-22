#pragma once

#include <Arduino.h>

extern bool sdReady;

void initializeSdCard();
void writeDataToSd();
String readDataSd();
int countLogs();
void refreshLogCount();
uint32_t freeStorageMegabytes();
void refreshFreeStorage();
void readRtc();
