#ifndef TASK_H_
#define TASK_H_

#if FAKE_ESP_IDF
#include <pthread.h>
#else  // FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"
#endif  // FAKE_ESP_IDF

namespace esp_cxx {

// RAII class for opening up an NVS handle.
class Task {
 public:
#if FAKE_ESP_IDF
  using PriorityType = int;
  using TaskHandle = pthread_t;
  static constexpr unsigned short kDefaultStackSize = 1024;

  // 1 is low and 99 is max for Linux SCHED_FIFO which is closes to
  // FreeRTOS's static priority based scheduling.
  static constexpr PriorityType kDefaultPrio = 1;
#else  // FAKE_ESP_IDF
  using PriorityType = UBaseType_t;
  using TaskHandle = TaskHandle_t;
  static constexpr unsigned short kDefaultStackSize = XT_STACK_EXTRA_CLIB;
  static constexpr PriorityType kDefaultPrio = ESP_TASK_MAIN_PRIO;
#endif  // FAKE_ESP_IDF

  Task() = default;
  Task(Task&& other)
    : task_handle_(other.task_handle_) {
      other.task_handle_ = {};
  }
  Task& operator=(Task&& rhs) {
    task_handle_ = rhs.task_handle_;
    rhs.task_handle_ = {};
    return *this;
  }

  // Creaes and starts the task.
  Task(void (*func)(void*),
       void* param,
       const char* name,
       unsigned short stackdepth = kDefaultStackSize,
       PriorityType priority = kDefaultPrio);

  // Auto-generate a thunk for object methods.
  template <typename T, void (T::*method)(void)>
  static void MethodThunk(void* param) {
    (static_cast<T*>(param)->*method)();
  }

  template <typename T, void (T::*method)(void)>
  static Task Create(T* obj,
                     const char* name,
                     unsigned short stackdepth = kDefaultStackSize,
                     PriorityType priority = kDefaultPrio) {
    return Task(&MethodThunk<T, method>, obj, name, stackdepth, priority);
  }

  // Terminates the task.
  ~Task();

  // Notifies the task.
  void Notify();

  // Blocks current task until a notification is received.
  static void CurrentTaskWait();

 private:
  TaskHandle task_handle_{};

  Task(Task&) = delete;
  void operator=(Task&) = delete;
};

}  // namespace esp_cxx

#endif  // TASK_H_
