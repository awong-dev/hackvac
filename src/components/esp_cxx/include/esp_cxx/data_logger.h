#ifndef ESPCXX_DATA_LOGGER_H_
#define ESPCXX_DATA_LOGGER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"

#include "esp_cxx/ringbuffer.h"

namespace esp_cxx {

// Starts an asynchronous ring-buffer based data logger for a single type of
// function. Useful for logging things like packet dumps off of the main
// handling thread so as to avoid missing protocol deadlines.
//
// Usage:
//   void LogPacket(std::unique_ptr<PacketType> packet);
//   DataLogger<std::unique_ptr<PacketType>, 50, &LogPacket> logger;
//     ...
//   logger.Log(std::move(some_packet));
template <typename T, size_t size, void(*LogFunc)(T)>
class DataLogger {
 public:
  explicit DataLogger(const char* name) : name_(name) {}

  void Init() {
    xTaskCreate(&LogTaskThunk, name_, XT_STACK_EXTRA_CLIB, this,
                ESP_TASK_MAIN_PRIO, &task_handle_);
  }

  void Log(T data) {
    data_log_.Put(std::move(data));
    xTaskNotify(task_handle_, 0, eNoAction);
  }

 private:
  static void LogTaskThunk(void* parameters) {
    DataLogger* self = static_cast<DataLogger*>(parameters);
    self->LogTaskRunLoop();
  }

  void LogTaskRunLoop() {
    for (;;) {
      T data;
      while (data_log_.Get(&data)) {
        LogFunc(std::move(data));
      }
      xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);
    }
  }

  // name for task and logging.
  const char* name_;

  // Handle of logging task.
  TaskHandle_t task_handle_ = nullptr;

  // Ring buffer for data to log.
  RingBuffer<T, size> data_log_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_DATA_LOGGER_H_
