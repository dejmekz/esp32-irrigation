#ifndef MQTT_COMMANDS_H
#define MQTT_COMMANDS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "TaskManager.h"

// Command handlers
bool handleValveControl(JsonDocument& doc, TaskManager& taskManager);
bool handlePumpControl(JsonDocument& doc);
bool handleTaskStart(TaskManager& taskManager);
bool handleTaskStop(TaskManager& taskManager);
bool handleSystemRestart(JsonDocument& doc);

// Response helpers
void publishCommandResponse(const char* topic, const char* cmd, bool success, const char* message);

#endif
