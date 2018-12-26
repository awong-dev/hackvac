#ifndef GTEST_MAIN
#include "cpp_entry.h"
void app_main() {
  cpp_entry();
}
#else
#include "cpp_test_entry.h"
void app_main() {
  cpp_test_entry();
}
#endif
