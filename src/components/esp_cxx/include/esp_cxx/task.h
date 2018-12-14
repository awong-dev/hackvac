#ifndef TASK_H_
#define TASK_H_

#if MOCK_ESP_IDF
#include <pthread.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"
#endif


namespace esp_cxx {

// RAII class for opening up an NVS handle.
class Task {
 public:
#if MOCK_ESP_IDF
  using PriorityType = int;
  using TaskHandle = pthread_t;
  static constexpr unsigned short kDefaultStackDepth = XT_STACK_EXTRA_CLIB;

  // 1 is low and 99 is max for Linux SCHED_FIFO which is closes to
  // FreeRTOS's static priority based scheduling.
  static constexpr PriorityType kDefaultPrio = 1;
#else
  using PriorityType = UBaseType_t;
  using TaskHandle = TaskHandle_t;
  static constexpr unsigned short kDefaultStackDepth = XT_STACK_EXTRA_CLIB;
  static constexpr PriorityType kDefaultPrio = ESP_TASK_MAIN_PRIO;
#endif

  // Creaes and starts the task.
  Task(void (*func)(void*),
       void* param,
       unsigned short stackdepth = kDefaultStackDepth,
       PriorityType priority = kDefaultPrio);

  // Auto-generate a thunk for object methods.
  template <typename T, void (T::*method)(void)>
  static void MethodThunk(void* param) {
    (static_cast<T*>(param)->*method)();
  }

  template <typename T, void (T::*method)(void)>
  Task(T* obj,
       unsigned short stackdepth = kDefaultStackDepth,
       PriorityType priority = kDefaultPrio)
    : Task(&MethodThunk<T, method>, obj, stackdepth, priority) {
  }

  // Terminates the task.
  ~Task();

  // Notifies the task.
  void Notify();

  // Blocks until a notification is received.
  void Wait();

 private:
  TaskHandle task_handle_{};

  Task(Task&) = delete;
  void operator=(Task&) = delete;
};

}  // namespace esp_cxx

#endif  // TASK_H_
