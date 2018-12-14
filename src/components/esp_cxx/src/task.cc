#include "esp_cxx/task.h"

namespace esp_cxx {

Task::Task(void (*func)(void*), void* param, unsigned short stackdepth,
           PriorityType priority) {
  xTaskCreate(func, "boo", stackdepth, param, priority, &task_handle_);
}

Task::~Task() {
  if (task_handle_) {
    vTaskDelete(task_handle_);
  }
}

void Task::Notify() {
  xTaskNotify(task_handle_, 0, eNoAction);
}

void Task::Wait() {
  xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);
}

}  // namespace esp_cxx
