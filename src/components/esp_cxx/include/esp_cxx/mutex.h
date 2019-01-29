#ifndef ESPCXX_MUTEX_H_
#define ESPCXX_MUTEX_H_

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <mutex>
#endif

namespace esp_cxx {

// Simple C++ wrapper over the portMUX_TYPE.
class Mutex {
 public:
#ifndef FAKE_ESP_IDF
  void lock() { portENTER_CRITICAL(&mux_); }
  void unlock() { portEXIT_CRITICAL(&mux_); }
#else
  void lock() { mux_.lock(); }
  void unlock() { mux_.unlock(); }
#endif

 private:
#ifndef FAKE_ESP_IDF
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#else
  std::mutex mux_;
#endif
};

}  // namespace esp_cxx

#endif  // ESPCXX_MUTEX_H_
