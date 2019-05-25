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
  // |event_manager| is where LogFunc is run.
  explicit AsyncDataLogger(EventManager* event_manager)
    : event_manager_(event_manager)  {
      PublishLog();
  }
  virtual ~AsyncDataLogger() = default;

  void Log(const char* tag, T data) override {
    data_log_.Put(std::move(data));
  }

 private:
  void PublishLog() {
    // Limit how many logs are sent at once to not allow packet logging
    // to completely DoS the network event loop.
    static constexpr int kMaxLogBurst = 5;
    static constexpr int kLogIntervalMs = 10;
    for (int i = 0; i < kMaxLogBurst; ++i) {
      auto data = data_log_.Get();
      if (!data) {
        break;
      }
      LogFunc(std::move(data.value()));
    }
    event_manager_->RunDelayed([=]{PublishLog();}, data_log_.NumItems() == 0 ? kLogIntervalMs : 0);
  }

  // EventManager to run the LogFunc() on.
  EventManager* event_manager_;

  // Ring buffer for data to log.
  DataBuffer<T, size> data_log_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_DATA_LOGGER_H_
