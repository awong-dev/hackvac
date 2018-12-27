#ifndef TASK_H_
#define TASK_H_

#include <utility>

#if FAKE_ESP_IDF
#include <pthread.h>
#else  // FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"
#endif  // FAKE_ESP_IDF

namespace esp_cxx {

// RAII class for opening up an NVS handle.
class TaskRef {
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
  static constexpr unsigned short kDefaultStackSize = XT_STACK_MIN_SIZE + XT_STACK_EXTRA_CLIB;
  static constexpr PriorityType kDefaultPrio = ESP_TASK_MAIN_PRIO;
#endif  // FAKE_ESP_IDF

  TaskRef() = default;
  TaskRef(TaskRef&& other)
    : task_handle_(other.task_handle_) {
      other.task_handle_ = {};
  }
  TaskRef& operator=(TaskRef&& rhs) {
    task_handle_ = rhs.task_handle_;
    rhs.task_handle_ = {};
    return *this;
  }

  static TaskRef CreateForCurrent() {
    TaskRef retval;
#ifndef FAKE_ESP_IDF
    retval.task_handle_ = xTaskGetCurrentTaskHandle();
#else
    retval.task_handle_ = pthread_self();
#endif
    return retval;
  }

  // Terminates the task.
  ~TaskRef();

  // Notifies the task.
  void Notify();

  // Stop the task.
  void Stop();

  // Blocks current task until a notification is received.
  static void CurrentTaskWait();

  // Sleeps the current thread for |delay_ms|.
  static void Delay(int delay_ms);

 protected:
  TaskHandle task_handle_{};

 private:
  TaskRef(TaskRef&) = delete;
  void operator=(TaskRef&) = delete;
};

// Same as TaskRef, but automatically closes the task.
class Task : public TaskRef {
 public:
  Task();
  Task(Task&& other)
    : TaskRef(std::move(other)) {
  }

  Task& operator=(Task&& rhs) {
    *static_cast<TaskRef*>(this) = std::move(rhs);
    return *this;
  }

  ~Task();

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

 private:
  Task(Task&) = delete;

  void operator=(Task&) = delete;
};

}  // namespace esp_cxx

#endif  // TASK_H_
