#include "sensors.h"

#include <Arduino.h>
#include <Wire.h>

#include "app_state.h"
#include "config.h"

Adafruit_BME680 bme;
BH1750 lightMeter(0x23);
bool bmeValid = false;
bool bhValid = false;
float averageSampleValues[5] = {0};

class AverageSamplingState
{
public:
  float temperature[10] = {0};
  float humidity[10] = {0};
  float pressure[10] = {0};
  float gas[10] = {0};
  float lux[10] = {0};
  int bmeIndex = 0;
  int bmeSampleCount = 0;
  int luxIndex = 0;
  int luxSampleCount = 0;
};

AverageSamplingState sampler;

void scanI2C()
{
  Serial.println("I2C scan:");
  for (uint8_t address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("Found 0x");
      if (address < 16)
      {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
}

void initializeSensors()
{
  bmeExists = bme.begin(0x76);
  if (bmeExists)
  {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setPressureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
  }
  else
  {
    Serial.println("BME680 not found at 0x76");
  }

  bhExists = lightMeter.begin();
  if (!bhExists)
  {
    Serial.println("BH1750 not found at 0x23");
  }
}

void readSensors()
{
  measurementCount = 0;
  measurementReady = false;
  environmentalAlert = false;
  bmeValid = false;
  bhValid = false;

  if (bmeExists && bme.performReading())
  {
    humidityPercent = bme.humidity;
    gasResistanceKOhm = bme.gas_resistance / 1000.0F;
    measurements[measurementCount++] = String(bme.temperature, 1);
    measurements[measurementCount++] = String(bme.humidity, 1);
    measurements[measurementCount++] = String(bme.pressure / 100.0F, 1);
    measurements[measurementCount++] = String(bme.gas_resistance / 1000.0F, 1);
    bmeValid = true;
  }

  if (bhExists)
  {
    ambientLightLux = lightMeter.readLightLevel();
    measurements[measurementCount++] = String(ambientLightLux, 1);
    bhValid = true;
  }

  measurementReady = measurementCount > 0;
  environmentalAlert = (bhExists && ambientLightLux < lightAlertThresholdLux) ||
                       (bmeExists && humidityPercent > humidityAlertThresholdPercent) ||
                       (bmeExists && gasResistanceKOhm > gasAlertThresholdKohm);
  digitalWrite(alertLEDPin, environmentalAlert ? HIGH : LOW);
}

void averageSensorSample()
{
  readSensors();

  if (bmeExists && bmeValid)
  {
    sampler.temperature[sampler.bmeIndex] = measurements[0].toFloat();
    sampler.humidity[sampler.bmeIndex] = measurements[1].toFloat();
    sampler.pressure[sampler.bmeIndex] = measurements[2].toFloat();
    sampler.gas[sampler.bmeIndex] = measurements[3].toFloat();
    sampler.bmeIndex = (sampler.bmeIndex + 1) % 10;
    if (sampler.bmeSampleCount < 10)
    {
      sampler.bmeSampleCount++;
    }

    float sumTemperature = 0;
    float sumHumidity = 0;
    float sumPressure = 0;
    float sumGas = 0;
    for (int index = 0; index < sampler.bmeSampleCount; index++)
    {
      sumTemperature += sampler.temperature[index];
      sumHumidity += sampler.humidity[index];
      sumPressure += sampler.pressure[index];
      sumGas += sampler.gas[index];
    }

    averageSampleValues[0] = sumTemperature / sampler.bmeSampleCount;
    averageSampleValues[1] = sumHumidity / sampler.bmeSampleCount;
    averageSampleValues[2] = sumPressure / sampler.bmeSampleCount;
    averageSampleValues[3] = sumGas / sampler.bmeSampleCount;
  }

  if (bhExists && bhValid)
  {
    sampler.lux[sampler.luxIndex] = measurements[4].toFloat();
    sampler.luxIndex = (sampler.luxIndex + 1) % 10;
    if (sampler.luxSampleCount < 10)
    {
      sampler.luxSampleCount++;
    }

    float sumLux = 0;
    for (int index = 0; index < sampler.luxSampleCount; index++)
    {
      sumLux += sampler.lux[index];
    }
    averageSampleValues[4] = sumLux / sampler.luxSampleCount;
  }
}
