#pragma once

#include <Arduino.h>

constexpr uint8_t DIRO = 8;
constexpr uint8_t TFT_CS = 10;
constexpr uint8_t TFT_RST = 6;
constexpr uint8_t TFT_DC = 7;
constexpr uint8_t TFT_SCLK = 13;
constexpr uint8_t TFT_MOSI = 11;

constexpr uint8_t button1Pin = 2;
constexpr uint8_t button2Pin = 3;
constexpr uint8_t alertLEDPin = 4;
constexpr uint8_t SDI_12_STATUS_LED_PIN = 5;

constexpr float lightAlertThresholdLux = 100.0F;
constexpr float humidityAlertThresholdPercent = 80.0F;
constexpr float gasAlertThresholdKohm = 100.0F;
constexpr unsigned long debounceMS = 50;
constexpr unsigned long displayInterval = 1000;

constexpr uint8_t SD_CS_PIN = A3;
constexpr uint8_t SOFT_MISO_PIN = 12;
constexpr uint8_t SOFT_MOSI_PIN = 11;
constexpr uint8_t SOFT_SCK_PIN = 13;

constexpr uint32_t TIMER_RC = 41016UL;
constexpr uint8_t TICKS_PER_LOG = 60;
constexpr unsigned long masterQueryIntervalMs = 30000;
constexpr unsigned long masterInitialDelayMs = 1000;
