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
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
void OnSigabrt(int signal) {
  void* trace[32];
  int frames = backtrace(&trace[0], 32);
  backtrace_symbols_fd(trace, frames, STDERR_FILENO);
}

int main(void) {
  signal(SIGABRT, &OnSigabrt);
  app_main();
  return 0;
}
#endif
