#ifndef ESPCXX_MUTEX_H_
#define ESPCXX_MUTEX_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esp_cxx {

class AutoMutex;

// Simple C++ wrapper over the portMUX_TYPE.
class Mutex {
 public:
  void Lock() { taskENTER_CRITICAL(&mux_); }
  void Unlock() { taskEXIT_CRITICAL(&mux_); }

 private:
  friend AutoMutex;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

// RAII class wrapping a taskENTER_CRITICAL/taskENTER_CRITICAL pair.
class AutoMutex {
 public:
  explicit AutoMutex(Mutex* mutex) : mutex_(mutex) { mutex_->Lock(); }
  ~AutoMutex() { mutex_->Unlock(); }

 private:
  Mutex* mutex_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_MUTEX_H_
