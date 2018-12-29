#ifndef ESPCXX_MUTEX_H_
#define ESPCXX_MUTEX_H_

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <pthread.h>
#endif

namespace esp_cxx {

class AutoMutex;

// Simple C++ wrapper over the portMUX_TYPE.
class Mutex {
 public:
#ifndef FAKE_ESP_IDF
  void Lock() { portENTER_CRITICAL(&mux_); }
  void Unlock() { portEXIT_CRITICAL(&mux_); }
#else
  void Lock() { pthread_mutex_lock(&mux_); }
  void Unlock() { pthread_mutex_unlock(&mux_); }
#endif

 private:
  friend AutoMutex;
#ifndef FAKE_ESP_IDF
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#else
  pthread_mutex_t mux_ = PTHREAD_MUTEX_INITIALIZER;
#endif
};

// RAII class wrapping a portENTER_CRITICAL/portENTER_CRITICAL pair.
class AutoMutex {
 public:
  explicit AutoMutex(Mutex* mutex) : mutex_(mutex) { mutex_->Lock(); }
  ~AutoMutex() { mutex_->Unlock(); }

 private:
  Mutex* mutex_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_MUTEX_H_
