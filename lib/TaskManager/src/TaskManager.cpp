#include "TaskManager.h"
#include <esp32-hal-log.h>

TaskManager::TaskManager(uint8_t hour, uint8_t minute, bool usePsram)
{
  if (usePsram && !psramFound())
  {
    log_e("PSRAM requested but not available!");
    usePsram = false;
  }
  else
  {
    log_i("PSRAM is available and will be used.");
    usePsram = usePsram;
  }

  _hour = hour;
  _minute = minute;

  stop();
}

TaskManager::~TaskManager()
{
  for (uint8_t i = 0; i < MAX_TASKS; i++)
  {
    if (_valve_settings[i] != nullptr)
    {
      free(_valve_settings[i]);  // Works for both malloc and ps_malloc
      _valve_settings[i] = nullptr;
    }
  }
  log_d("TaskManager destroyed, memory freed");
}

void TaskManager::setStartTime(uint8_t hour, uint8_t minute)
{
  _hour = hour;
  _minute = minute;
}

valve_setting_t* TaskManager::actualValveSetting()
{
  return _actual_valve_settings;
}

uint8_t TaskManager::timeLeft()
{
  return _actual_delay;
}

const char* TaskManager::executeAt() { 
  static char info[6];
  snprintf(info, sizeof(info), "%02d:%02d", _hour, _minute);
  return info;
}

const char* TaskManager::statusMessage() { 
  static char info[21];

  if (_current_valve_setting < 0 || _actual_valve_settings == nullptr) {
    snprintf(info, sizeof(info), "Cekam do: %02d:%02d", _hour, _minute);
    return info;
  }

  if (_pump_is_ready == 0) {
    snprintf(info, sizeof(info), "Cekam na cerpadlo");
    return info;
  }
  
  //Ukol:00 - 00 [00]
  snprintf(info, sizeof(info), "Ukol:%2d - %02d [%02d]",_current_valve_setting,  _actual_delay, _actual_valve_settings->duration);
  return info;
}


bool TaskManager::isRunning()
{
  return _actual_valve_settings != nullptr;
}

bool TaskManager::isPumpOn()
{
  return _pump_is_ready != 0;
}

bool TaskManager::setValveSetting(uint8_t idx, uint16_t valves, uint8_t duration)
{
  valve_setting_t *setting;
  if (usePsram)
  {
    setting = (valve_setting_t *)ps_malloc(sizeof(valve_setting_t));
  }
  else
  {
    setting = (valve_setting_t *)malloc(sizeof(valve_setting_t));
  }

  if (setting != nullptr)
  {
    setting->valves = valves;
    setting->duration = duration;
    _valve_settings[idx] = setting;
    return true;
  }
  else
  {
    log_e("Failed to allocate memory for valve setting at index %d", idx);
    return false;
  }
}

void TaskManager::start()
{
  log_d("Starting ...");
    _current_valve_setting = 0; // Start from the first task
    if (_onPumpSet)
    {
      _onPumpSet(true); // Turn on the pump
    }
}

void TaskManager::stop(){
    log_d("Stopping ...");
    _actual_valve_settings = nullptr;
    _pump_is_ready = 0;
    if (_onPumpSet)
    {
      _onPumpSet(false); // Turn off the pump
    }
    _current_valve_setting = -1;
}

void TaskManager::loop(uint8_t hour, uint8_t minute)
{
  log_d("TaskManager loop: %02d:%02d, executed:%d, pump state:%d", hour, minute, _current_valve_setting, _pump_is_ready);

  if (_current_valve_setting == -1)
  {
    if (hour != _hour || minute != _minute)
    {
      return; // Not the time to start tasks yet
    }

    log_d("Initial time match at %02d:%02d", hour, minute);
    start();
  }

  if (_pump_is_ready == 0 && _onIsReady)
  {
    _pump_is_ready = _onIsReady() ? 1 : 0;
    log_d("Pump readiness check: %d", _pump_is_ready);
    return; // Wait until the pump is ready
  }

  if (_actual_delay > 0)
  {
    _actual_delay--;
    log_d("Decrementing actual delay: %d", _actual_delay);
    return; // Still waiting
  }
  
  _current_valve_setting++;
  if (_current_valve_setting >= MAX_TASKS || _valve_settings[_current_valve_setting] == nullptr)
  {
    stop();
    return;
  }

  _actual_valve_settings = _valve_settings[_current_valve_setting];
  _actual_delay = _actual_valve_settings->duration;
 
  log_d("Switching to valve setting %d: valves=%d, duration=%d", _current_valve_setting, _actual_valve_settings->valves, _actual_delay);
 
  if (_onValveSet)
  {
    _onValveSet(_actual_valve_settings); // Call the callback to change valve state
  }
}

void TaskManager::setCallbacks(ValveSetCallback setValve, PumpCallback setPump, std::function<bool ()> isReady)
{
  _onValveSet = setValve; // Store callback
  _onPumpSet = setPump;   // Store callback
  _onIsReady = isReady;   // Store callback
}