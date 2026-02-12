
#include <Arduino.h>
#include <esp32-hal-log.h>
#include <esp32fota.h>
#include <esp_task_wdt.h>
#include <Wire.h>
#include <RTClib.h>

#include "TaskManager.h"
#include "config.h"
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "mqtt_commands.h"
#include "config_storage.h"
#include "lcd.h"
#include "utils.h"
#include "valves.h"
#include "pump.h"

char DeviceName[20]; //Wifi hostname - max 32 chars
char mqtt_topic_tasks[36];
char mqtt_topic_will[35];
char mqtt_topic_cmnd[36];
char mqtt_topic_conf[36];
char mqtt_topic_state[37];
char mqtt_topic_wifi[36];

volatile bool alarm1Triggered = true;

// https://github.com/lrswss/esp32-irrigation-automation
// https://github.com/espressif/arduino-esp32/blob/2.0.14/libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino
// https://circuitdigest.com/microcontroller-projects/esp32-timers-and-timer-interrupts

esp32FOTA esp32_FOTA(FOTA_FIRMWARE_TYPE, FIRMWARE_VERSION, false, true);

TaskManager taskManager(20,0,false); // Enable PSRAM if available
ConfigStorage configStorage;
RTC_DS3231 rtc;

DateTime alarm1Time = DateTime(2025, 1, 1, 0, 0, 0);

bool clockSynced = false;
bool rtcAvailable = false;

void synchronize_clock_from_ntp();
void mqtt_message_handler(char *topic, byte *message, unsigned int length);
void setupVariables();
void setRTC();
void setTaskManager();
void displayReset(uint8_t minute);
void publishWifiStatus(int minutes, String timestampMsg);
void publishTaskStatus(TaskManager& taskManager, const String& timestampMsg);
void checkIfExistNewFirmware(int minutes);
void mqtt_setup_after_connect();
void clearAlarm();

void onPumpSet(bool onOff);
bool isPumpReady();
void setValvesStatus(valve_setting_t *setting);

void onAlarm()
{
  alarm1Triggered = true;
}

void setup()
{
  setupVariables();

  wifi_init(DeviceName, WIFI_SSID, WIFI_PASSWORD);
  mqtt_init(DeviceName, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, mqtt_topic_will);
  mqtt_set_callback(mqtt_message_handler, mqtt_setup_after_connect);

  esp32_FOTA.setManifestURL(FOTA_MANIFEST_URL);
  esp32_FOTA.printConfig();  

  log_i("Starting ESP32C3_IRRIGATION...");

  setTaskManager();

  Wire.begin(5, 4); // SDA on GPIO 5, SCL on GPIO 4

  lcd_init();
  valves_init();
  pump_init();
  setRTC();

  // Configure hardware watchdog timer (30 seconds timeout)
  esp_task_wdt_init(30, true);  // 30 seconds, panic on timeout
  esp_task_wdt_add(NULL);       // Add current task to WDT
  log_i("Hardware watchdog enabled (30s timeout)");
}

void printDateTime()
{
  if (!rtcAvailable) {
    return;  // Skip if RTC not available
  }

  DateTime now = rtc.now();
  float temp = rtc.getTemperature();

  lcd_print_date_time(3, 1, now);
  lcd_print_temp(4, 1, temp);
}

void loop()
{
  esp_task_wdt_reset();  // Reset watchdog every loop iteration

  static unsigned long prevLoopTimer = 0;  // Only this should be static

  // These must update every iteration - NOT static!
  bool is_wifi_connected = wifi_loop();
  bool is_mqtt_connected = is_wifi_connected && mqtt_loop();

    // run tasks once every second
  if (millis() - prevLoopTimer >= 1000) {
    prevLoopTimer = millis();

    if (is_wifi_connected) {
      synchronize_clock_from_ntp();
    }
    printDateTime();
  }

  if (alarm1Triggered)
  {
    log_i("Triggered Alarm ...");
  
    clearAlarm();
  
    DateTime now = rtc.now();

    uint8_t minutes = now.minute();
    uint8_t hours = now.hour();

    taskManager.loop(hours, minutes); // Call task manager to check for tasks
    displayReset(minutes); // Reset display at the start of each hour    

    if (is_wifi_connected && !taskManager.isRunning()) {
      checkIfExistNewFirmware(minutes); // Check for new firmware if no task is running
    }
        
    char timestampMsg[20];
    snprintf(timestampMsg, 20, "%02d.%02d.%04d %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());

    lcd_print_task(taskManager);

    if (is_mqtt_connected)
    {
      publishWifiStatus(now.minute(), timestampMsg);
      publishTaskStatus(taskManager, timestampMsg);
    }

    log_i("Wifi: %d, MQTT: %d", is_wifi_connected, is_mqtt_connected);
  }
  
  vTaskDelay(100 / portTICK_PERIOD_MS); // Delay to avoid blocking the loop
}

void clearAlarm() {      
    if (rtc.alarmFired(1))
    {
      rtc.clearAlarm(1);      
      alarm1Triggered = false; // Reset the alarm trigger flag
      log_d("Alarm cleared");
    }
}

void synchronize_clock_from_ntp()
{
  if (clockSynced)
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

void mqtt_message_handler(char *topic, byte *message, unsigned int length)
{
  log_i("Message arrived on topic: %s", topic);

  String messageStr;
  for (int i = 0; i < length; i++)
  {
    messageStr += (char)message[i];
  }
  log_i("Message: %s", messageStr.c_str());

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, messageStr);

  if (error)
  {
    log_e("JSON parse error: %s", error.c_str());
    publishCommandResponse(mqtt_topic_state, "unknown", false, "Invalid JSON");
    return;
  }

  if (!doc.containsKey("cmd"))
  {
    log_e("No 'cmd' field in message");
    publishCommandResponse(mqtt_topic_state, "unknown", false, "Missing cmd field");
    return;
  }

  const char* cmd = doc["cmd"];
  bool success = false;
  String responseMsg = "Unknown command";

  if (strcmp(topic, mqtt_topic_cmnd) == 0)
  {
    // Command topic handling
    if (strcmp(cmd, "valve_control") == 0)
    {
      success = handleValveControl(doc, taskManager);
      responseMsg = success ? "Valve control applied" : "Valve control failed";
    }
    else if (strcmp(cmd, "pump_control") == 0)
    {
      success = handlePumpControl(doc);
      responseMsg = success ? "Pump control applied" : "Pump control failed";
    }
    else if (strcmp(cmd, "task_start") == 0)
    {
      success = handleTaskStart(taskManager);
      responseMsg = success ? "Tasks started" : "Tasks already running";
    }
    else if (strcmp(cmd, "task_stop") == 0)
    {
      success = handleTaskStop(taskManager);
      responseMsg = success ? "Tasks stopped" : "No tasks running";
    }
    else if (strcmp(cmd, "system_restart") == 0)
    {
      publishCommandResponse(mqtt_topic_state, cmd, true, "Restarting...");
      handleSystemRestart(doc);  // This will restart the system
      return;
    }
  }
  else if (strcmp(topic, mqtt_topic_conf) == 0)
  {
    // Configuration topic handling (will be implemented with NVS storage)
    log_i("Config update received - not yet implemented");
    responseMsg = "Config updates not implemented yet";
  }

  publishCommandResponse(mqtt_topic_state, cmd, success, responseMsg.c_str());
}

void setupVariables()
{
  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint32_t deviceId = 0;
  for (int i = 0; i < 25; i = i + 8)
  {
    deviceId |= ((chipid >> (40 - i)) & 0xff) << i;
  }

  snprintf(DeviceName, sizeof(DeviceName), "irrigation_%08X", deviceId);                   // 19 + 1
  snprintf(mqtt_topic_tasks, sizeof(mqtt_topic_tasks), "irrigation/%s/tasks", DeviceName); // 11 + 20 + 6 = 37
  snprintf(mqtt_topic_will, sizeof(mqtt_topic_will), "irrigation/%s/LWT", DeviceName);     // 11 + 20 + 4 = 35
  snprintf(mqtt_topic_cmnd, sizeof(mqtt_topic_cmnd), "irrigation/%s/cmnd", DeviceName);    // 11 + 20 + 5 = 36
  snprintf(mqtt_topic_conf, sizeof(mqtt_topic_conf), "irrigation/%s/conf", DeviceName);
  snprintf(mqtt_topic_state, sizeof(mqtt_topic_state), "irrigation/%s/state", DeviceName); // 11 + 20 + 6 = 37
  snprintf(mqtt_topic_wifi, sizeof(mqtt_topic_wifi), "irrigation/%s/wifi", DeviceName);    // 11 + 20 + 5 = 36
  snprintf(DeviceName, sizeof(DeviceName), "irrigation-%08X", deviceId);
}

void setRTC()
{
  const int MAX_RETRIES = 5;
  int retries = 0;

  while (!rtc.begin())
  {
    retries++;
    log_e("RTC initialization failed (attempt %d/%d)", retries, MAX_RETRIES);

    if (retries >= MAX_RETRIES)
    {
      log_e("CRITICAL: RTC not found after %d attempts - system will continue WITHOUT scheduling", MAX_RETRIES);
      rtcAvailable = false;
      alarm1Triggered = false;  // Prevent alarm processing
      return;  // Continue without RTC instead of hanging
    }

    delay(1000);  // Wait before retry
  }

  log_i("RTC initialized successfully");
  rtcAvailable = true;

  rtc.disable32K();

  pinMode(RTC_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), onAlarm, FALLING);

  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  rtc.writeSqwPinMode(DS3231_OFF);

  // Only one alarm can be set at a time
  rtc.disableAlarm(2);

  // Schedule Alarm1 to fire when the minutes match
  if (!rtc.setAlarm1(alarm1Time, DS3231_A1_Second))
  { // this mode triggers the alarm when the minutes match
    log_e("Error, alarm wasn't set!");
  }
  else
  {
    log_i("Alarm 1 will happen at specified time");
  }
}

void setTaskManager()
{
  taskManager.setCallbacks(setValvesStatus, onPumpSet, isPumpReady);

  // Try to load from NVS
  if (configStorage.getTaskCount() > 0)
  {
    log_i("Loading tasks from NVS");
    configStorage.loadToTaskManager(taskManager);
  }
  else
  {
    // Use defaults and save to NVS
    log_i("No saved tasks, using defaults");

    uint8_t hour = 20, minute = 0;
    configStorage.saveSchedule(hour, minute);
    taskManager.setStartTime(hour, minute);

    taskManager.setValveSetting(0, decodeBinaryString("oxo xxx xxx xxx"), 20);
    configStorage.saveTask(0, decodeBinaryString("oxo xxx xxx xxx"), 20);

    taskManager.setValveSetting(1, decodeBinaryString("xox xxx xxx xox"), 17);
    configStorage.saveTask(1, decodeBinaryString("xox xxx xxx xox"), 17);

    taskManager.setValveSetting(2, decodeBinaryString("xxx xxx oox xxx"), 15);
    configStorage.saveTask(2, decodeBinaryString("xxx xxx oox xxx"), 15);

    taskManager.setValveSetting(3, decodeBinaryString("xxx xxx xxx oxo"), 15);
    configStorage.saveTask(3, decodeBinaryString("xxx xxx xxx oxo"), 15);
  }
}

void displayReset(uint8_t minute)
{
  // Removed hourly LCD reinit - only init in setup()
  // If LCD issues occur, implement proper error detection and recovery in lcd.cpp
}

void publishTaskStatus(TaskManager& taskManager, const String& timestampMsg)
{
  JsonDocument doc;
  doc["time"] = timestampMsg;
  doc["at"] = taskManager.executeAt();
  doc["run"] = taskManager.isRunning();
  doc["pump"] = taskManager.isPumpOn();

  valve_setting_t* tsk = taskManager.actualValveSetting();
  if (tsk != nullptr) {
    doc["valve"]["valves"] = tsk->valves;
    doc["valve"]["duration"] = tsk->duration;
    doc["valve"]["left"] = taskManager.timeLeft();
  }

  mqtt_publish_json(mqtt_topic_tasks, doc);
}

// Publish WiFi status and device information every 10 minutes
void publishWifiStatus(int minutes, String timestampMsg)
{
  if ((minutes % 10 == 0))
  {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["time"] = timestampMsg;

    wifi_add_info(doc); // Add WiFi info to the JSON document
    mqtt_publish_json(mqtt_topic_wifi, doc);

    doc.clear(); // Clear the document for the next use

    float temp = rtc.getTemperature();
    char vBuffer[10];
    dtostrf(temp, 5, 2, vBuffer);

    doc["time"] = timestampMsg;
    doc["name"] = DeviceName;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["temp"] = temp;

    // doc["chip"]["revision"] = ESP.getChipRevision();
    // doc["chip"]["model"] = ESP.getChipModel();
    // doc["chip"]["cores"] = ESP.getChipCores();

    // doc["heap"]["total"] = ESP.getHeapSize();
    // doc["heap"]["free"] = ESP.getFreeHeap();
    // doc["heap"]["minFree"] = ESP.getMinFreeHeap();
    // doc["heap"]["maxAlloc"] = ESP.getMaxAllocHeap();

    // doc["psram"]["size"] = ESP.getPsramSize();
    // doc["psram"]["available"] = ESP.getFreePsram();
    // doc["psram"]["minFree"] = ESP.getMinFreePsram();
    // doc["psram"]["maxAlloc"] = ESP.getMaxAllocPsram();

    mqtt_publish_json(mqtt_topic_state, doc);
  }
}

void checkIfExistNewFirmware(int minutes)
{
  if (minutes % 10 != 0)
  {
    return;
  }

  esp32_FOTA.handle(); // Check for updates
}

void mqtt_setup_after_connect()
{
  mqtt_subscribe(mqtt_topic_cmnd, 0);
  mqtt_subscribe(mqtt_topic_conf, 0);
}

void onPumpSet(bool onOff)
{
  log_i("pump set to: %d", onOff);
  valves_off();
  pump_on(onOff);
}

bool isPumpReady()
{
  return pump_is_ready();
}

void setValvesStatus(valve_setting_t *setting)
{
  if (setting == nullptr)
  {
    log_e("setValvesStatus: setting is null");
    return;
  }

  log_i("Setting valves to: %s for %d minutes", toValveString(setting->valves).c_str(), setting->duration);
  valves_write(setting->valves);
}