#include "cpp_test_entry.h"

#include "esp_cxx/logging.h"

#include <cstdio>
#include "gmock/gmock.h"

void cpp_test_entry() {
  int argc = 1;
  const auto arg0 = "PlatformIO";
  char* argv0 = const_cast<char*>(arg0);
  char** argv = &argv0;

  // Since Google Mock depends on Google Test, InitGoogleMock() is
  // also responsible for initializing Google Test.  Therefore there's
  // no need for calling testing::InitGoogleTest() separately.
  testing::InitGoogleMock(&argc, argv);
  int result = RUN_ALL_TESTS();
  ESP_LOGI("TestMain", "Test result %d", result);
}
