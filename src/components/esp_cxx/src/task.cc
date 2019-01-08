#include <cassert>

#include "esp_cxx/task.h"

#ifdef FAKE_ESP_IDF
#include <unistd.h>
#endif

namespace {
#ifdef FAKE_ESP_IDF
void KillSignalHandler(int junk) {
  pthread_exit(0);
}

struct PthreadState {
  PthreadState(void (*f)(void*), void* p)
    : thread_main(f), param(p) {
  }
  void (*thread_main)(void*);
  void* param;
};

void DeleteMe(void* ptr) {
  delete reinterpret_cast<PthreadState*>(ptr);
}

void* PThreadWrapperFunc(void* param) {
  signal(SIGUSR1, &KillSignalHandler);
  PthreadState* state = static_cast<PthreadState*>(param);
  pthread_cleanup_push(&DeleteMe, state);
  state->thread_main(state->param);
  pthread_cleanup_pop(1);
  return nullptr;
}

#endif  // FAKE_ESP_IDF

}  // namespace

namespace esp_cxx {

Task::Task() = default;

Task::Task(void (*func)(void*), void* param, const char* name,
           unsigned short stack_size, PriorityType priority) {
// TODO(awong): This needs to prevent func from returning.
#ifndef FAKE_ESP_IDF
  xTaskCreate(func, name, stack_size, param, priority, &task_handle_);
#else  // FAKE_ESP_IDF
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

  sched_param sched_param;
  sched_param.sched_priority = priority;
  pthread_attr_setschedparam(&attr, &sched_param);
  pthread_attr_setstacksize(&attr, stack_size);

  pthread_create(&task_handle_, &attr, &PThreadWrapperFunc, new PthreadState(func, param));

  pthread_attr_destroy(&attr);
#endif  // FAKE_ESP_IDF
}

TaskRef::~TaskRef() = default;

void TaskRef::Notify() {
#ifndef FAKE_ESP_IDF
  xTaskNotify(task_handle_, 0, eNoAction);
#else  // FAKE_ESP_IDF
  std::unique_lock<std::mutex> lock(notification_lock_);
  is_notified_ = true;
  notification_cv_.notify_all();
#endif  // FAKE_ESP_IDF
}

void TaskRef::Stop() {
  if (task_handle_) {
#ifndef FAKE_ESP_IDF
    vTaskDelete(task_handle_);
#else  // FAKE_ESP_IDF
    pthread_kill(task_handle_, SIGUSR1);
#endif  // FAKE_ESP_IDF
  }
}

void TaskRef::Wait() {
#ifndef FAKE_ESP_IDF
  xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);
#else  // FAKE_ESP_IDF
  // TODO(awong): This emulation is incorrect as each TaskRef
  // object has a different notification condition variable
  // rather than a shared one.
  std::unique_lock<std::mutex> lock(notification_lock_);
  while (!is_notified_) {
    notification_cv_.wait(lock);
  }
  is_notified_ = false;
#endif
}

void TaskRef::Delay(int delay_ms) {
#ifndef FAKE_ESP_IDF
  vTaskDelay(delay_ms / portTICK_PERIOD_MS);
#else
  struct timespec delay_spec = {
    delay_ms / 1000,
    (delay_ms % 1000) * 1000,
  };
  while (nanosleep(&delay_spec, &delay_spec) != 0);
#endif
}

Task::~Task() {
  Stop();
}

}  // namespace esp_cxx
