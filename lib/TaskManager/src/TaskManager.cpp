#include "TaskManager.h"
#include <esp32-hal-log.h>

Task *findTaskByIdAndType(std::vector<Task *> &taskList, int id, TaskType type)
{
  for (Task *t : taskList)
  {
    if (t->id == id && t->type == type)
    {
      return t;
    }
  }
  return nullptr;
}

Task *createNewTask(std::vector<Task *> &taskList, bool usePsram, int id, TaskType type, uint16_t valves, uint8_t hour, uint8_t minute, bool pumpOn)
{
  Task *task;
  if (usePsram)
  {
    task = (Task *)ps_malloc(sizeof(Task));
  }
  else
  {
    task = new Task;
  }

  if (task)
  {
    task->id = id;
    task->valves = valves;
    task->hour = hour;
    task->minute = minute;
    task->pumpOn = pumpOn;
    task->type = type;
    taskList.push_back(task);
    log_i("Created new task with ID: %d, Type: %d, Valves: %04X, Hour: %d, Minute: %d, PumpOn: %d",
          id, static_cast<int>(type), valves, hour, minute, pumpOn);
  }
  else
  {
    log_e("Error allocating memory for task!");
    return nullptr; // Return null if allocation failed
  }

  return task;
}

TaskManager::TaskManager(bool usePsram)
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
}

TaskManager::~TaskManager()
{
  clearTasks();
}

void TaskManager::addOrUpdateScheduleTask(int id, uint8_t hour, uint8_t minute)
{
  Task *existing = findTaskByIdAndType(taskList, id, TaskType::Scheduler);
  if (existing)
  {
    log_i("Task %d already exists, updating hour: %d, minute: %d", id, hour, minute);
    existing->hour = hour;
    existing->minute = minute;
  }
  else
  {
    createNewTask(taskList, usePsram, id, TaskType::Scheduler, 0, hour, minute, false);
  }
}

void TaskManager::addOrUpdateValeTask(int id, uint16_t valves, uint8_t minute)
{
  Task *existing = findTaskByIdAndType(taskList, id, TaskType::Valve);
  if (existing)
  {
    log_i("Task %d already exists, updating valves: %04X, minute: %d", id, valves, minute);
    existing->valves = valves;
    existing->minute = minute;
  }
  else
  {
    createNewTask(taskList, usePsram, id, TaskType::Valve, valves, 0, minute, false);
  }
}

void TaskManager::addOrUpdatePumpTask(int id, bool pumpOn, uint8_t duration)
{
  Task *existing = findTaskByIdAndType(taskList, id, TaskType::Pump);
  if (existing)
  {
    log_i("Task %d already exists, updating pump: %d", id, pumpOn);
    existing->pumpOn = pumpOn;
    existing->minute = duration; // Update duration if needed
  }
  else
  {
    createNewTask(taskList, usePsram, id, TaskType::Pump, 0, 0, duration, pumpOn);
  }
}

void TaskManager::clearTasks()
{
  for (Task *t : taskList)
  {
    if (usePsram)
    {
      free(t);
    }
    else
    {
      delete t;
    }
  }
  taskList.clear();
  currentTask = 0;
}

void TaskManager::activateTask()
{
  if (taskList.empty())
  {
    log_i("No tasks available to activate.");
    log_i("TaskList size: %d, currentTask: %d", taskList.size(), currentTask);
    log_i("TaskList begin: %d, end: %d", taskList.begin(), taskList.end());
    return;
  }

  currentTask++;

  if (currentTask >= taskList.size())
  {
    log_i("Reached the end of task list, resetting to first task.");
    currentTask = 0; // Reset to first task
  }

  log_i("TaskList size: %d, currentTask: %d", taskList.size(), currentTask);

  Task *task = taskList[currentTask];
  if (task)
  {
    delayMin = task->minute;
    actualTask = task;

    log_i("Actived task with ID: %d, Type: %d, Valves: %04X, Hour: %d, Minute: %d, PumpOn: %d",
          task->id, static_cast<int>(task->type), task->valves, task->hour, task->minute, task->pumpOn);

    return;
  }
  else
  {
    actualTask = nullptr;
    log_e("Error: Task pointer at index %d is null!", currentTask);
  }
}

void TaskManager::loop(uint8_t hour, uint8_t minute)
{
  log_d("TaskManager loop: %02d:%02d", hour, minute);
  if (hour == 0 && minute == 0)
  {
    currentTask = -1;     // Reset current task at midnight
    actualTask = nullptr; // Clear the current task
    delayMin = 0;         // Reset delay
    log_i("Resetting tasks at midnight.");
  }

  if (actualTask != nullptr)
  {
    log_d("Current task ID: %d, Type: %d, Valves: %04X, Hour: %d, Minute: %d, PumpOn: %d, delayMin: %d",
          actualTask->id, static_cast<int>(actualTask->type), actualTask->valves,
          actualTask->hour, actualTask->minute, actualTask->pumpOn, delayMin);

    if (actualTask->type == TaskType::Scheduler)
    {
      // If the task is a scheduler, check if the hour and minute match
      if (actualTask->hour != hour || actualTask->minute != minute)
      {
        log_d("Skipping task %d, scheduled for %02d:%02d", actualTask->id, actualTask->hour, actualTask->minute);
        return; // Skip if the time does not match
      }
    } else if (delayMin > 0)
    {
      delayMin--;
      return; // Skip if delay is not yet zero
    }
  }

  activateTask();

  if (onTaskDone)
  {
    onTaskDone(actualTask); // Call the callback
  }
}

void TaskManager::setOnTaskActive(TaskDoneCallback callback)
{
  onTaskDone = callback; // Store the callback
}

