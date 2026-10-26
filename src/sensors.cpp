#include "sensors.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>

#include "app_state.h"
#include "config.h"

Adafruit_BME680 bme;
BH1750 lightMeter(0x23);
bool bmeValid = false;
bool bhValid = false;
float averageSampleValues[5] = {0};

class AverageSamplingClass
{
  public:
    float tempSamples[10];
    float humiditySamples[10];
    float pressureSamples[10];
    float gasSamples[10];
    float luxSamples[10];
    int bmeIndex = 0;
    int bmeSampleCount = 0;
    int luxIndex = 0;
    int luxSampleCount = 0;
  AverageSamplingClass() :
      tempSamples{0},
      humiditySamples{0},
      pressureSamples{0},
      gasSamples{0},
      luxSamples{0},
      bmeIndex(0),
      bmeSampleCount(0),
      luxIndex(0),
      luxSampleCount(0)
    {}
};

AverageSamplingClass sensorSampler;

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

  bhExists = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
  if (!bhExists)
  {
    Serial.println("BH1750 not found at 0x23");
  }
  readSensors();
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
    environmentalRisk = calculateEnvironmentalRisk(
        calculateTemperatureRisk(bme.temperature),
        calculateHumidityRisk(bme.humidity),
        calculatePressureRisk(bme.pressure / 100.0F));
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

int calculateTemperatureRisk(float temperature)
{
  if (temperature >= 10 && temperature <= 25) return 0;
  if (temperature > 25 && temperature < 45) return constrain((temperature - 25) * 5, 0, 100);
  if (temperature > 0 && temperature < 10) return constrain((10 - temperature) * 10, 0, 100);
  return 100;
}

int calculateHumidityRisk(float humidity)
{
  if (humidity >= 30 && humidity <= 60) return 0;
  if (humidity > 60 && humidity < 100) return constrain((humidity - 60) * 2.5F, 0, 100);
  if (humidity > 0 && humidity < 30) return constrain((30 - humidity) * (100.0F / 30.0F), 0, 100);
  return 100;
}

int calculatePressureRisk(float pressure)
{
  if (pressure >= 1000) return 0;
  if (pressure <= 960) return 100;
  return constrain((1000 - pressure) * 2.5F, 0, 100);
}

int calculateEnvironmentalRisk(int temperatureRisk, int humidityRisk, int pressureRisk)
{
  return (temperatureRisk + humidityRisk + pressureRisk) / 3;
}

void AverageSensorSample()
{
  readSensors();

  // Keep the most recent ten readings so the display and logger can use a
  // steadier value than a single noisy I2C conversion.
  if (bmeExists && bmeValid)
  {
    sensorSampler.tempSamples[sensorSampler.bmeIndex] = measurements[0].toFloat();
    sensorSampler.humiditySamples[sensorSampler.bmeIndex] = measurements[1].toFloat();
    sensorSampler.pressureSamples[sensorSampler.bmeIndex] = measurements[2].toFloat();
    sensorSampler.gasSamples[sensorSampler.bmeIndex] = measurements[3].toFloat();

    sensorSampler.bmeIndex = (sensorSampler.bmeIndex + 1) % 10;

    if (sensorSampler.bmeSampleCount < 10)
    {
      sensorSampler.bmeSampleCount++;
    }

    float sumTemp = 0, sumHum = 0, sumPres = 0, sumGas = 0;
    for (int index = 0; index < sensorSampler.bmeSampleCount; index++)
    {
      sumTemp += sensorSampler.tempSamples[index];
      sumHum += sensorSampler.humiditySamples[index];
      sumPres += sensorSampler.pressureSamples[index];
      sumGas += sensorSampler.gasSamples[index];
    }

    if (sensorSampler.bmeSampleCount > 0)
    {
      averageSampleValues[0] = sumTemp / sensorSampler.bmeSampleCount;
      averageSampleValues[1] = sumHum / sensorSampler.bmeSampleCount;
      averageSampleValues[2] = sumPres / sensorSampler.bmeSampleCount;
      averageSampleValues[3] = sumGas / sensorSampler.bmeSampleCount;
    }
  }

  if (bhExists && bhValid)
  {
    sensorSampler.luxSamples[sensorSampler.luxIndex] = measurements[4].toFloat();

    sensorSampler.luxIndex = (sensorSampler.luxIndex + 1) % 10;

    if (sensorSampler.luxSampleCount < 10)
    {
      sensorSampler.luxSampleCount++;
    }

    float sumLux = 0;
    for (int index = 0; index < sensorSampler.luxSampleCount; index++)
    {
      sumLux += sensorSampler.luxSamples[index];
    }

    if (sensorSampler.luxSampleCount > 0)
    {
      averageSampleValues[4] = sumLux / sensorSampler.luxSampleCount;
    }
  }
}
