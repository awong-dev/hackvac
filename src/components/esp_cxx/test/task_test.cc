#include "esp_cxx/task.h"

#include <atomic>
#include <cstdio>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace esp_cxx;

struct TaskData {
  std::atomic_bool flag{false};
  TaskRef handle_ = Task::CreateForCurrent();
};

void TaskMain(void* param) {
  TaskData *data = static_cast<TaskData*>(param);
  data->flag = true;
  data->handle_.Notify();
  for (;;) {
    esp_cxx::Task::Delay(1000);
  }
}

TEST(Task, Basic) {
  TaskData data;
  esp_cxx::Task t(&TaskMain, &data, "test");
  data.handle_.Wait();
  ASSERT_TRUE(data.flag);
}
