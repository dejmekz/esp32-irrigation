#pragma once

#include <Arduino.h>


#define MAX_TASKS 5

typedef struct
{
  uint16_t valves;
  uint8_t duration;
} valve_setting_t;

typedef void (*ValveSetCallback)(valve_setting_t *);
typedef void (*PumpCallback)(bool);

class TaskManager
{
public:
  TaskManager(uint8_t hour, uint8_t minute, bool usePsram = false);
  ~TaskManager();

  void setStartTime(uint8_t hour, uint8_t minute);  
  bool setValveSetting(uint8_t idx, uint16_t valves, uint8_t duration);
  void setCallbacks(ValveSetCallback setValve, PumpCallback setPump, std::function<bool ()> isReady);

  valve_setting_t* actualValveSetting();

  void loop(uint8_t hour, uint8_t minute); // Should be called from loop()

  void start();
  void stop();

  uint8_t timeLeft();
  bool isRunning();
  bool isPumpOn();

  const char* executeAt();
  const char* statusMessage();
  
private:
  uint8_t _hour;
  uint8_t _minute;

  uint8_t _pump_is_ready = 0;

  valve_setting_t* _valve_settings[MAX_TASKS];
  bool usePsram;

  valve_setting_t* _actual_valve_settings = nullptr; // Pointer to the current task
  int _current_valve_setting;
  uint8_t _actual_delay;

  ValveSetCallback _onValveSet = nullptr; // Store callback
  PumpCallback _onPumpSet = nullptr; // Store callback
  std::function<bool ()> _onIsReady = nullptr; // Store callback
};
