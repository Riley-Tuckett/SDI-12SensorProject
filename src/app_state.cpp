#include "app_state.h"

int hours = 0;
int minutes = 0;
int seconds = 0;

volatile bool logRequested = false;
volatile bool manualLogRequested = false;
volatile bool isTimerForSensorSamplingTriggered = false;

volatile bool button1EdgePending = false;
volatile bool button1InterruptLocked = false;

volatile bool button2EdgePending = false;
volatile bool button2InterruptLocked = false;
bool showSystemInfo = false;

String command;
char deviceAddress = '1';
const String deviceIdentification = "14ENG20009105916092";
String measurements[5];
String sdi12PendingMessage;
uint8_t measurementCount = 0;
bool bmeExists = false;
bool bhExists = false;
bool measurementReady = false;
bool externalWeatherDataReady = false;
String externalWeatherData;
bool environmentalAlert = false;
float ambientLightLux = 0.0F;
float humidityPercent = 0.0F;
float gasResistanceKOhm = 0.0F;
unsigned long previousDisplayUpdate = 0;

volatile bool sdi12Active = false;
volatile bool sdi12MasterBusy = false;
volatile bool sdi12State = false;
volatile int sdi12BlinkCount = 0;
