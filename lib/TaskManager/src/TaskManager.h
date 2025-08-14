#pragma once

#include <Arduino.h>
#include <vector>

enum class TaskType : uint8_t
{
  None,
  Valve,
  Pump,
  Scheduler,
};

// Forward declaration of Task
struct Task
{
  int id;
  TaskType type = TaskType::None;
  uint16_t valves;
  uint8_t hour;
  uint8_t minute;
  bool pumpOn = false;
};

typedef void (*TaskAction)();
typedef void (*TaskDoneCallback)(Task *);

class TaskManager
{
public:
  TaskManager(bool usePsram = false);
  ~TaskManager();

  void addOrUpdateScheduleTask(int id, uint8_t hour, uint8_t minute);
  void addOrUpdateValeTask(int id, uint16_t valves, uint8_t minute);
  void addOrUpdatePumpTask(int id, bool pumpOn, uint8_t duration);
  void clearTasks();
  void loop(uint8_t hour, uint8_t minute); // Should be called from loop()
  void setOnTaskActive(TaskDoneCallback callback);

  Task* getCurrentTask() const
  {
    return actualTask;
  }

  uint8_t timeLeft()
  {
    return delayMin;
  }

  uint8_t getCurrentTaskIndex() const
  {
    return currentTask;
  }

  uint8_t getTaskCount() const
  {
    return taskList.size();
  }

private:
  std::vector<Task *> taskList;
  bool usePsram;

  Task* actualTask = nullptr; // Pointer to the current task
  int currentTask = -1;
  uint8_t delayMin = 0;

  TaskDoneCallback onTaskDone = nullptr; // Store callback

  void activateTask();
};
