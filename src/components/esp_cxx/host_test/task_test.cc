#include "esp_cxx/task.h"

#include <atomic>
#include <cstdio>

std::atomic_bool flag{0};

// TODO(awong): This should never return.
void TaskMain(void* param) {
  *static_cast<std::atomic_bool*>(param) = true;
}

int main(void) {
  {
    std::atomic_bool flag{false};
    esp_cxx::Task t(&TaskMain, &flag, "test");
    while (flag != true);
    fprintf(stderr, "task success\n");
  }
  return 0;
}
