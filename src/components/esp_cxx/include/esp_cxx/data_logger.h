#ifndef ESPCXX_DATA_LOGGER_H_
#define ESPCXX_DATA_LOGGER_H_

#include "esp_cxx/task.h"
#include "esp_cxx/data_buffer.h"

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
    task_ = Task::Create<DataLogger, &DataLogger::LogTaskRunLoop>(this, name_);
  }

  void Log(T data) {
    data_log_.Put(std::move(data));
    task_.Notify();
  }

 private:
  void LogTaskRunLoop() {
    for (;;) {
      T data;
      while (data_log_.Get(&data)) {
        LogFunc(std::move(data));
      }
      Task::CurrentTaskWait();
    }
  }

  // name for task and logging.
  const char* name_;

  // Handle of logging task.
  Task task_;

  // Ring buffer for data to log.
  DataBuffer<T, size> data_log_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_DATA_LOGGER_H_
