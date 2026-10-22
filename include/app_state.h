#pragma once

#include <Arduino.h>

extern int hours;
extern int minutes;
extern int seconds;

extern volatile bool logRequested;
extern volatile bool manualLogRequested;
extern volatile bool isTimerForSensorSamplingTriggered;

extern volatile bool button1EdgePending;
extern volatile bool button1InterruptLocked;

extern volatile bool button2EdgePending;
extern volatile bool button2InterruptLocked;
extern bool showSystemInfo;

extern String command;
extern char deviceAddress;
extern const String deviceIdentification;
extern String measurements[5];
extern String sdi12PendingMessage;
extern uint8_t measurementCount;
extern bool bmeExists;
extern bool bhExists;
extern bool measurementReady;
extern bool environmentalAlert;
extern float ambientLightLux;
extern float humidityPercent;
extern float gasResistanceKOhm;
extern unsigned long previousDisplayUpdate;

extern volatile bool sdi12Active;
extern volatile bool sdi12State;
extern volatile int sdi12BlinkCount;
