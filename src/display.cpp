#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <math.h>

#include "app_state.h"
#include "config.h"
#include "storage.h"

namespace
{
  Adafruit_ST7735 display(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

  String measurementOrUnavailable(uint8_t index)
  {
    return index < measurementCount ? measurements[index] : String("--");
  }
}

void initializeDisplay()
{
  display.initR(INITR_BLACKTAB);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);
}

void displayDashboard()
{
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("Sensor Dashboard");
  display.setCursor(5, 15);
  display.setTextColor(environmentalAlert ? ST77XX_RED : ST77XX_GREEN);
  display.println(environmentalAlert ? "[ALERT]" : "[OK]");
  display.setTextColor(ST77XX_WHITE);

  display.setCursor(5, 30);
  display.print("Temp: ");
  display.print(measurementOrUnavailable(0));
  display.println(" C");
  display.setCursor(5, 40);
  display.print("Humidity: ");
  display.print(measurementOrUnavailable(1));
  display.println(" %");
  display.setCursor(5, 50);
  display.print("Pressure: ");
  display.print(measurementOrUnavailable(2));
  display.println(" hPa");
  display.setCursor(5, 60);
  display.print("Gas: ");
  display.print(measurementOrUnavailable(3));
  display.println(" kOhm");
  display.setCursor(5, 70);
  display.print("Light: ");
  display.print(measurementOrUnavailable(4));
  display.println(" lux");
  display.setCursor(5, 90);
  display.print("Logging: ");
  display.setTextColor(sdReady ? ST77XX_GREEN : ST77XX_RED);
  display.println(sdReady ? "ACTIVE" : "OFF");
  display.setTextColor(ST77XX_WHITE);
}

void displaySystemInfo()
{
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("System Info");
  display.setCursor(5, 25);
  display.print("Total Logs: ");
  display.println(countLogs());
  display.setCursor(5, 35);
  display.print("Free Space: ");
  display.print(freeStorageMegabytes());
  display.println(" MB");
  display.setCursor(5, 50);
  display.print("Storage: ");
  display.setTextColor(sdReady ? ST77XX_GREEN : ST77XX_RED);
  display.println(sdReady ? "READY" : "ERROR");
  display.setTextColor(ST77XX_WHITE);
}

void displayEnvironmentalRisk()
{
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("Environmental Risk");

  constexpr int centerX = 80, centerY = 95, radius = 45;
  for (int angle = 0; angle <= 180; angle++)
  {
    uint16_t colour = angle <= 60 ? ST77XX_RED : (angle < 120 ? ST77XX_YELLOW : ST77XX_GREEN);
    float radians = angle * PI / 180.0F;
    display.fillCircle(centerX + cos(radians) * radius, centerY - sin(radians) * radius, 2, colour);
  }
  float needleRadians = (180.0F - environmentalRisk * 1.8F) * PI / 180.0F;
  display.drawLine(centerX, centerY, centerX + cos(needleRadians) * 35,
                   centerY - sin(needleRadians) * 35, ST77XX_WHITE);
  display.fillCircle(centerX, centerY, 3, ST77XX_WHITE);
  display.setCursor(55, 103);
  display.print("Risk: "); display.print(environmentalRisk); display.println("%");
  display.setCursor(60, 115);
  display.setTextColor(environmentalRisk <= 33 ? ST77XX_GREEN : (environmentalRisk <= 66 ? ST77XX_YELLOW : ST77XX_RED));
  display.print(environmentalRisk <= 33 ? "SAFE" : (environmentalRisk <= 66 ? "CAUTION" : "DANGER"));
}
