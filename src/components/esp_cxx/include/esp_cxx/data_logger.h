#ifndef ESPCXX_DATA_LOGGER_H_
#define ESPCXX_DATA_LOGGER_H_

#include "esp_cxx/task.h"
#include "esp_cxx/data_buffer.h"

namespace esp_cxx {

template <typename T>
class DataLogger {
 public:
  virtual ~DataLogger() = default;
  virtual void Init() {}
  virtual void Log(const char* tag, T data) {}
};

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
class AsyncDataLogger : public DataLogger<T> {
 public:
  explicit AsyncDataLogger(const char* name) : name_(name) {}
  virtual ~AsyncDataLogger() = default;

  void Init() override {
    task_ = Task::Create<AsyncDataLogger, &AsyncDataLogger::LogTaskRunLoop>(this, name_);
  }

  void Log(const char* tag, T data) override {
    data_log_.Put(std::move(data));
    task_.Notify();
  }

 private:
  void LogTaskRunLoop() {
    for (;;) {
      while (auto data = data_log_.Get()) {
        LogFunc(std::move(data.value()));
      }
      Task::CurrentTaskWait();
    }
  }

  // name for task and logging.
  // TODO(awong): This should not create its own task.
  const char* name_;

  // Handle of logging task.
  Task task_;

  // Ring buffer for data to log.
  DataBuffer<T, size> data_log_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_DATA_LOGGER_H_
