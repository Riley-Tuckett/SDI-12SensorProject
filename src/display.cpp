#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

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
