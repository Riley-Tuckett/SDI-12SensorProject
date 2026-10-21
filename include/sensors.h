#pragma once

#include <Adafruit_BME680.h>
#include <BH1750.h>

extern Adafruit_BME680 bme;
extern BH1750 lightMeter;
extern bool bmeValid;
extern bool bhValid;
extern float averageSampleValues[5];

void scanI2C();
void initializeSensors();
void readSensors();
void averageSensorSample();
