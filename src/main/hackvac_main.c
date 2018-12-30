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

#ifdef HOST_BUILD
int main(void) {
  app_main();
  return 0;
}
#endif
