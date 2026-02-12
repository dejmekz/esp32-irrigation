#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include "TaskManager.h"

class ConfigStorage
{
public:
  ConfigStorage();

  bool begin();
  void end();

  // Schedule settings
  bool saveSchedule(uint8_t hour, uint8_t minute);
  bool loadSchedule(uint8_t& hour, uint8_t& minute);

  // Task settings
  bool saveTask(uint8_t idx, uint16_t valves, uint8_t duration);
  bool loadTask(uint8_t idx, uint16_t& valves, uint8_t& duration);
  uint8_t getTaskCount();

  // Apply to TaskManager
  void loadToTaskManager(TaskManager& tm);

  // Reset to defaults
  void reset();

private:
  Preferences preferences;
  static const char* NAMESPACE;
};

#endif
