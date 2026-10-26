#include "storage.h"

#include <Arduino.h>
#include <RTClib.h>
#include "SdFat.h"

#include "app_state.h"
#include "config.h"
#include "sensors.h"

namespace
{
  SdFs sd;
  FsFile dataLog;
  RTC_DS1307 rtc;
  SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;
  #define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(4), &softSpi)
}

bool sdReady = false;
int cachedLogCount = 0;
uint32_t cachedFreeStorageMegabytes = 0;

void ReadRTC()
{
  DateTime now = rtc.now();
  hours = now.hour();
  minutes = now.minute();
  seconds = now.second();
}

String ReadDataSD()
{
  String fileContent;
  FsFile file = sd.open("LOG.CSV", O_READ);
  if (file)
  {
    while (file.available())
    {
      char character = file.read();
      Serial.write(character);
      fileContent += character;
    }
    file.close();
  }
  return fileContent;
}

void WriteDataToSD()
{
  readSensors();
  ReadRTC();
  if (!sdReady)
  {
    Serial.println("Skipping SD write: SD card is not initialized.");
    return;
  }

  FsFile file = sd.open("LOG.CSV", O_RDWR | O_CREAT | O_APPEND);
  if (file)
  {
    for (uint8_t index = 0; index < 5; index++)
    {
      if (index > 0)
      {
        file.print(',');
      }
      file.print(measurements[index]);
    }
    file.print(',');
    file.print(hours);
    file.print(',');
    file.print(minutes);
    file.print(',');
    file.println(seconds);
    file.sync();
    file.close();
    cachedLogCount++;
    Serial.println("Successfully wrote to SD card.");
  }
  else
  {
    Serial.println("Failed to open file for writing.");
  }
}

void InitialiseSDCard()
{
  Serial.println("Initializing SD card using Software SPI...");
  rtc.begin();

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  sdReady = sd.begin(SD_CONFIG);
  if (!sdReady)
  {
    Serial.println("SD card initialization failed! Check wiring or card format (FAT32).");
    return;
  }

  Serial.println("SD card initialized successfully!");
  dataLog = sd.open("LOG.CSV", O_RDWR | O_CREAT | O_APPEND);
  if (dataLog)
  {
    if (dataLog.size() == 0)
    {
      dataLog.println(F("Tempurature,Humidity,Pressure,GasResistance,Lux,hourRTC,minuteRTC,secondRTC"));
      dataLog.sync();
    }
    dataLog.close();
    Serial.println(F("Test data written to LOG.CSV successfully!"));
  }
  else
  {
    Serial.println(F("Error: Could not open LOG.CSV for writing."));
  }

  refreshLogCount();
  refreshFreeStorage();
}

int countLogs()
{
  return cachedLogCount;
}

void refreshLogCount()
{
  cachedLogCount = 0;
  if (!sdReady)
  {
    return;
  }

  FsFile file = sd.open("LOG.CSV", O_READ);
  if (!file)
  {
    return;
  }

  int lineCount = 0;
  while (file.available())
  {
    if (file.read() == '\n')
    {
      lineCount++;
    }
  }
  file.close();
  cachedLogCount = lineCount > 0 ? lineCount - 1 : 0;
}

uint32_t freeStorageMegabytes()
{
  // The expensive FAT scan happens during setup; the display only reads this cache.
  return cachedFreeStorageMegabytes;
}

void refreshFreeStorage()
{
  if (!sdReady)
  {
    cachedFreeStorageMegabytes = 0;
    return;
  }

  uint64_t freeBytes = static_cast<uint64_t>(sd.freeClusterCount()) * sd.bytesPerCluster();
  cachedFreeStorageMegabytes = static_cast<uint32_t>(freeBytes / 1000000ULL);
}
