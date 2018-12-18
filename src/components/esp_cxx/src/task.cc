#include <cassert>

#include "esp_cxx/task.h"

namespace {
#if FAKE_ESP_IDF
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

Task::Task(void (*func)(void*), void* param, const char* name,
           unsigned short stack_size, PriorityType priority) {
#if FAKE_ESP_IDF
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
#else  // FAKE_ESP_IDF
  xTaskCreate(func, name, stack_size, param, priority, &task_handle_);
#endif  // FAKE_ESP_IDF
}

Task::~Task() {
  if (task_handle_) {
#if FAKE_ESP_IDF
    pthread_kill(task_handle_, SIGUSR1);
#else  // FAKE_ESP_IDF
    vTaskDelete(task_handle_);
#endif  // FAKE_ESP_IDF
  }
}

void Task::Notify() {
#if FAKE_ESP_IDF
  // TODO(awong): Signal!
#else  // FAKE_ESP_IDF
  xTaskNotify(task_handle_, 0, eNoAction);
#endif  // FAKE_ESP_IDF
}

void Task::CurrentTaskWait() {
#if FAKE_ESP_IDF
  // TODO(awong): Sleep!
#else  // FAKE_ESP_IDF
  xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);
#endif  // FAKE_ESP_IDF
}

}  // namespace esp_cxx
