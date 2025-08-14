
#include <Arduino.h>
#include <esp32-hal-log.h>
#include <esp32fota.h>
#include <Wire.h>
#include <RTClib.h>

#include "Esp32Mqtt.h"
#include "TaskManager.h"
#include "config.h"
#include "lcd.h"
#include "utils.h"
#include "valves.h"
#include "pump.h"

char DeviceName[20];
char mqtt_topic_valves[37];
char mqtt_topic_will[35];
char mqtt_topic_cmnd[36];
char mqtt_topic_state[37];
char mqtt_topic_wifi[36];

bool alarm1Triggered = true;


// https://en.cppreference.com/w/cpp/io/c/fprintf
// https://github.com/lrswss/esp32-irrigation-automation
// https://github.com/espressif/arduino-esp32/blob/2.0.14/libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino
// https://circuitdigest.com/microcontroller-projects/esp32-timers-and-timer-interrupts

esp32FOTA esp32_FOTA(FOTA_FIRMWARE_TYPE, FIRMWARE_VERSION, false, true);

TaskManager taskManager(false); // Enable PSRAM if available
Esp32Mqtt espMqtt(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, DeviceName);
RTC_DS3231 rtc;

DateTime alarm1Time = DateTime(2025, 1, 1, 0, 0, 0);

bool clockSynced = false;

void syncWithNTP();
void mqttCallback(char *topic, byte *message, unsigned int length);
void setupVariables();
void onTaskActive(Task *t);
void setRTC();
void setTaskManager();
void displayReset(uint8_t hour, uint8_t minute);
void publishWifiStatus(int minutes, String timestampMsg);
void publishTaskStatus(Task* task, String timestampMsg);
void checkIfExistNewFirmware(int hours, int minutes);

void onAlarm() {
  alarm1Triggered = true;
}

void setup()
{
  esp32_FOTA.setManifestURL(FOTA_MANIFEST_URL);
  esp32_FOTA.printConfig();

  setupVariables();

  log_i("Starting ESP32C3_IRRIGATION...");

  setTaskManager();
  espMqtt.setup(mqtt_topic_will, mqttCallback);

  Wire.begin(5, 4); // SDA on GPIO 5, SCL on GPIO 4

  lcd_init();
  valves_init();
  pump_init();
  setRTC();
}

void printDateTime()
{
  DateTime now = rtc.now();
  float temp = rtc.getTemperature();

  lcd_print_date_time(3,1,now);
  lcd_print_temp(4,1,temp);
}

void loop()
{
  espMqtt.loop(); // Zpracování MQTT zpráv

  syncWithNTP();

  // One minute alarm check
  if (alarm1Triggered)
  {
    alarm1Triggered = false; // Reset the alarm trigger flag
    if (rtc.alarmFired(1)) {
        rtc.clearAlarm(1);
        log_d("Alarm cleared");
    }

    DateTime now = rtc.now();
    log_i("Triggered Alarm ...");
    uint8_t minutes = now.minute();
    uint8_t hours = now.hour();

    taskManager.loop(hours, minutes); // Call task manager to check for tasks

    Task* actualTask = taskManager.getCurrentTask();

    displayReset(hours, minutes);
    checkIfExistNewFirmware(actualTask ,hours, minutes);

    char timestampMsg[20];
    snprintf(timestampMsg, 20, "%02d.%02d.%04d %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());

    lcd_print_task(1,1, actualTask, taskManager.timeLeft());

    if (espMqtt.isMqttConnected())
    {
      publishWifiStatus(now.minute(), timestampMsg);
      publishTaskStatus(actualTask, timestampMsg);
    }
    else
    {
      log_e("MQTT not connected, skipping publish.");
    }

    log_i("Wifi connected: %s", espMqtt.isWifiConnected() ? "Yes" : "No");
    log_i("MQTT connected: %s", espMqtt.isMqttConnected() ? "Yes" : "No");
  }

  printDateTime();
  delay(1000);
}

void syncWithNTP()
{
  if (clockSynced || !espMqtt.isWifiConnected())
  {
    return;
  }

  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", NTP_SERVER);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    DateTime now = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    now = now + TimeSpan(0, 0, 0, 2); // Adjust to the next second to avoid issues with RTC initialization
    rtc.adjust(now);                  // Nastavení času do RTC

    log_i("RTC synchronized with NTP server.");
    clockSynced = true;
  }
}

void mqttCallback(char *topic, byte *message, unsigned int length)
{
  log_i("Message arrived on topic: %s", topic);
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    messageTemp += (char)message[i];
  }
  log_i("Message: %s", messageTemp);
}

void setupVariables()
{
  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint32_t deviceId = 0;
  for (int i = 0; i < 25; i = i + 8)
  {
    deviceId |= ((chipid >> (40 - i)) & 0xff) << i;
  }

  snprintf(DeviceName, sizeof(DeviceName), "irrigation_%08X", deviceId);                      // 19 + 1
  snprintf(mqtt_topic_valves, sizeof(mqtt_topic_valves), "irrigation/%s/valves", DeviceName); // 11 + 20 + 6 = 37
  snprintf(mqtt_topic_will, sizeof(mqtt_topic_will), "irrigation/%s/LWT", DeviceName);        // 11 + 20 + 4 = 35
  snprintf(mqtt_topic_cmnd, sizeof(mqtt_topic_cmnd), "irrigation/%s/cmnd", DeviceName);       // 11 + 20 + 5 = 36
  snprintf(mqtt_topic_state, sizeof(mqtt_topic_state), "irrigation/%s/state", DeviceName);    // 11 + 20 + 6 = 37
  snprintf(mqtt_topic_wifi, sizeof(mqtt_topic_wifi), "irrigation/%s/wifi", DeviceName);       // 11 + 20 + 5 = 36
  snprintf(DeviceName, sizeof(DeviceName), "irrigation-%08X", deviceId);
}

void onTaskActive(Task *t)
{
  valves_off();

  if (t == nullptr || t->type == TaskType::None)
  {
    log_e("onTaskActive: Task is null or of type None.");
    return; // Do nothing if task is null or of type None
  }

  if (t->type == TaskType::Scheduler)
  {
    log_i("Task %d will completed at %02d:%02d", t->id, t->hour, t->minute);
  }
  else if (t->type == TaskType::Valve)
  {
    String valveStr = toValveString(t->valves);
    log_i("Valve task %d, valves set to: %s", t->id, valveStr.c_str());
    valves_write(t->valves);
  }
  else if (t->type == TaskType::Pump)
  {
    log_i("Pump task %d, pump set to: %s", t->id, t->pumpOn ? "ON" : "OFF");
    pump_state(t->pumpOn); // Set the pump state
  }
}

void setRTC() {
  if(!rtc.begin()) {
    log_i("Couldn't find RTC!");
    while (1) delay(10);
  }

  rtc.disable32K();

  pinMode(RTC_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), onAlarm, FALLING);  

  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  rtc.writeSqwPinMode(DS3231_OFF);

  // Only one alarm can be set at a time
  rtc.disableAlarm(2);

  // Schedule Alarm1 to fire when the minutes match
  if(!rtc.setAlarm1(alarm1Time, DS3231_A1_Second)) {  // this mode triggers the alarm when the minutes match
    log_e("Error, alarm wasn't set!");
  }else {
    log_i("Alarm 1 will happen at specified time");
  }
}

void setTaskManager() {
  taskManager.setOnTaskActive(onTaskActive);
  taskManager.addOrUpdateScheduleTask(0, 20, 58);
  taskManager.addOrUpdatePumpTask(1, true, 10); // Pump ON and wait 10 minutes to pump water to the valves
//                                                BOX    -1- -2- -3- -4-
  taskManager.addOrUpdateValeTask(2, decodeBinaryString("oxo xxx xxx xxx"), 20); // 20
  taskManager.addOrUpdateValeTask(3, decodeBinaryString("xox xxx xxx xox"), 17); // 17
  taskManager.addOrUpdateValeTask(4, decodeBinaryString("xxx xxx oox xxx"), 15); // 15
  taskManager.addOrUpdateValeTask(5, decodeBinaryString("xxx xxx xxx oxo"), 15); // 15
  taskManager.addOrUpdatePumpTask(6, false, 0); // Pump OFF
}

void displayReset(uint8_t hour, uint8_t minute) {
  // Reset the display to the specified hour and minute

  if (minute != 0) {
    return; // Reset each hour
  }

  lcd_init();
}

void publishTaskStatus(Task* task, String timestampMsg) {
  if (task == nullptr) {
    return; // No task to publish
  }

  JsonDocument doc;
  doc["time"] = timestampMsg;
  doc["task"]["id"] = task->id;
  doc["task"]["type"] = static_cast<uint8_t>(task->type);

  if (task->type == TaskType::Scheduler) {
    doc["scheduler"]["hour"] = task->hour;
    doc["scheduler"]["min"] = task->minute;
  } else if (task->type == TaskType::Pump) {
    doc["pump"] = task->pumpOn ? "ON" : "OFF";
  } else if (task->type == TaskType::Valve) {
    doc["valves"] = task->valves;
    doc["duration"]["total"] = task->minute;
    doc["duration"]["remains"] = taskManager.timeLeft();
  }

  espMqtt.publishJson(mqtt_topic_valves, doc);
}

// Publish WiFi status and device information every 10 minutes
void publishWifiStatus(int minutes, String timestampMsg)
{
  if ((minutes % 10 == 0))
  {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["time"] = timestampMsg;

    espMqtt.addWifiInfo(doc); // Add WiFi info to the JSON document
    espMqtt.publishJson(mqtt_topic_wifi, doc);

    doc.clear(); // Clear the document for the next use
  
    float temp = rtc.getTemperature();
    char vBuffer[10];
    dtostrf(temp, 5, 2, vBuffer);

    doc["time"] = timestampMsg;
    doc["name"] = DeviceName;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["temp"] = temp;

    //doc["chip"]["revision"] = ESP.getChipRevision();
    //doc["chip"]["model"] = ESP.getChipModel();
    //doc["chip"]["cores"] = ESP.getChipCores();

    //doc["heap"]["total"] = ESP.getHeapSize();
    //doc["heap"]["free"] = ESP.getFreeHeap();
    //doc["heap"]["minFree"] = ESP.getMinFreeHeap();
    //doc["heap"]["maxAlloc"] = ESP.getMaxAllocHeap();

    //doc["psram"]["size"] = ESP.getPsramSize();
    //doc["psram"]["available"] = ESP.getFreePsram();
    //doc["psram"]["minFree"] = ESP.getMinFreePsram();
    //doc["psram"]["maxAlloc"] = ESP.getMaxAllocPsram();

    espMqtt.publishJson(mqtt_topic_state, doc);
  }
}

void checkIfExistNewFirmware(Task* actualTask, int hours, int minutes)
{
  if (actualTask != nullptr) {
    if (actualTask->type != TaskType::Scheduler)
    {
      return; // Only check for firmware updates if the current task is a scheduler
    }
  }

  if (!espMqtt.isWifiConnected())
  {
    return;
  }

  if (minutes % 10 != 0)
  {
    return;
  }

  esp32_FOTA.handle(); // Check for updates
}