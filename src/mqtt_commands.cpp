#include "mqtt_commands.h"
#include "mqtt_handler.h"
#include "pump.h"
#include "valves.h"
#include "utils.h"
#include <esp32-hal-log.h>

bool handleValveControl(JsonDocument& doc, TaskManager& taskManager)
{
  if (!doc["params"].containsKey("valves") || !doc["params"].containsKey("duration"))
  {
    log_e("Invalid valve_control params");
    return false;
  }

  uint16_t valves = doc["params"]["valves"];
  uint8_t duration = doc["params"]["duration"];

  if (duration == 0)
  {
    valves_off();
    log_i("Valves turned off via MQTT");
    return true;
  }

  // Manual valve control - bypass task manager
  if (taskManager.isRunning())
  {
    log_w("Task running, stopping before manual control");
    taskManager.stop();
  }

  valves_write(valves);
  log_i("Manual valve control: %s for %d minutes", toValveString(valves).c_str(), duration);

  return true;
}

bool handlePumpControl(JsonDocument& doc)
{
  if (!doc["params"].containsKey("state"))
  {
    log_e("Invalid pump_control params");
    return false;
  }

  bool state = doc["params"]["state"];
  pump_on(state);
  log_i("Pump set to %s via MQTT", state ? "ON" : "OFF");

  return true;
}

bool handleTaskStart(TaskManager& taskManager)
{
  if (taskManager.isRunning())
  {
    log_w("Tasks already running");
    return false;
  }

  taskManager.start();
  log_i("Tasks started via MQTT");
  return true;
}

bool handleTaskStop(TaskManager& taskManager)
{
  if (!taskManager.isRunning())
  {
    log_w("No tasks running");
    return false;
  }

  taskManager.stop();
  log_i("Tasks stopped via MQTT");
  return true;
}

bool handleSystemRestart(JsonDocument& doc)
{
  int delay_sec = doc["params"].containsKey("delay") ? (int)doc["params"]["delay"] : 5;

  log_i("System restart requested, restarting in %d seconds", delay_sec);

  delay_sec = constrain(delay_sec, 1, 60);  // Limit to 1-60 seconds
  delay(delay_sec * 1000);

  ESP.restart();
  return true;  // Never reached
}

void publishCommandResponse(const char* topic, const char* cmd, bool success, const char* message)
{
  JsonDocument doc;
  doc["cmd"] = cmd;
  doc["success"] = success;
  doc["message"] = message;
  doc["timestamp"] = millis();

  mqtt_publish_json(topic, doc);
}
