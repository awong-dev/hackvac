#include "esp_cxx/task.h"

#include <atomic>
#include <cstdio>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

std::atomic_bool flag{0};

// TODO(awong): This should never return.
void TaskMain(void* param) {
  *static_cast<std::atomic_bool*>(param) = true;
}

TEST(Task, Basic) {
  std::atomic_bool flag{false};
  esp_cxx::Task t(&TaskMain, &flag, "test");
  sleep(5);

  ASSERT_TRUE(flag);
}
