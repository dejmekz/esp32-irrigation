#include "config_storage.h"
#include "utils.h"
#include <esp32-hal-log.h>

const char* ConfigStorage::NAMESPACE = "irrigation";

ConfigStorage::ConfigStorage() {}

bool ConfigStorage::begin()
{
  return preferences.begin(NAMESPACE, false);  // Read-write mode
}

void ConfigStorage::end()
{
  preferences.end();
}

bool ConfigStorage::saveSchedule(uint8_t hour, uint8_t minute)
{
  if (!begin()) return false;

  preferences.putUChar("sched_hour", hour);
  preferences.putUChar("sched_min", minute);

  end();
  log_i("Saved schedule: %02d:%02d", hour, minute);
  return true;
}

bool ConfigStorage::loadSchedule(uint8_t& hour, uint8_t& minute)
{
  if (!begin()) return false;

  hour = preferences.getUChar("sched_hour", 20);    // Default 20:00
  minute = preferences.getUChar("sched_min", 0);

  end();
  log_i("Loaded schedule: %02d:%02d", hour, minute);
  return true;
}

bool ConfigStorage::saveTask(uint8_t idx, uint16_t valves, uint8_t duration)
{
  if (idx >= MAX_TASKS) return false;
  if (!begin()) return false;

  char keyValves[16], keyDuration[16];
  snprintf(keyValves, sizeof(keyValves), "task%d_valves", idx);
  snprintf(keyDuration, sizeof(keyDuration), "task%d_dur", idx);

  preferences.putUShort(keyValves, valves);
  preferences.putUChar(keyDuration, duration);

  end();
  log_i("Saved task %d: valves=%d, duration=%d", idx, valves, duration);
  return true;
}

bool ConfigStorage::loadTask(uint8_t idx, uint16_t& valves, uint8_t& duration)
{
  if (idx >= MAX_TASKS) return false;
  if (!begin()) return false;

  char keyValves[16], keyDuration[16];
  snprintf(keyValves, sizeof(keyValves), "task%d_valves", idx);
  snprintf(keyDuration, sizeof(keyDuration), "task%d_dur", idx);

  valves = preferences.getUShort(keyValves, 0);
  duration = preferences.getUChar(keyDuration, 0);

  end();

  if (valves == 0 && duration == 0) return false;  // No task stored

  log_i("Loaded task %d: valves=%d, duration=%d", idx, valves, duration);
  return true;
}

uint8_t ConfigStorage::getTaskCount()
{
  uint8_t count = 0;
  uint16_t valves;
  uint8_t duration;

  for (uint8_t i = 0; i < MAX_TASKS; i++)
  {
    if (loadTask(i, valves, duration))
    {
      count++;
    }
    else
    {
      break;  // Stop at first empty task
    }
  }

  return count;
}

void ConfigStorage::loadToTaskManager(TaskManager& tm)
{
  uint8_t hour, minute;
  if (loadSchedule(hour, minute))
  {
    tm.setStartTime(hour, minute);
  }

  for (uint8_t i = 0; i < MAX_TASKS; i++)
  {
    uint16_t valves;
    uint8_t duration;

    if (loadTask(i, valves, duration))
    {
      tm.setValveSetting(i, valves, duration);
      log_i("Applied task %d to TaskManager: %s, %d min", i, toValveString(valves).c_str(), duration);
    }
    else
    {
      break;  // No more tasks
    }
  }
}

void ConfigStorage::reset()
{
  if (!begin()) return;

  preferences.clear();
  end();

  log_i("Configuration reset to defaults");
}
