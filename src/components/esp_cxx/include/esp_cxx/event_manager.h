#ifndef ESPCXX_EVENT_MANAGER_H_
#define ESPCXX_EVENT_MANAGER_H_

#include <array>
#include <functional>

#include "esp_cxx/mutex.h"
#include "mongoose.h"

namespace esp_cxx {

class EventManager {
 public:
  using Duration = std::chrono::steady_clock::duration;
  using TimePoint = std::chrono::steady_clock::time_point;

  class Poller {
   public:
    virtual ~Poller() = default;
  };

  // Will run |task| as soon as possible.
  void Add(std::function<void(void)> closure);

  // Will run |task| at least milliseconds after this is called.
  void AddDelayed(std::function<void(void)> closure, int milliseconds);

  // Waits for next I/O event or task before waking up.
  void Run();

//  mg_mgr* underlying_manager() { return &event_manager_; }

 protected:
  EventManager() = default;
  virtual ~EventManager() = default;

  virtual void Poll(Duration timeout_ms) = 0;
  virtual void Wake() = 0;

 private:
  struct ClosureEntry {
    std::function<void(void)> thunk;
    TimePoint run_at = TimePoint::min();
    bool operator<(const ClosureEntry& other) const { return run_at < other.run_at; }
  };
  using ClosureList = std::array<ClosureEntry, 10>;

  // Places all closures that are scheduled before |now| in to |to_run|.
  // The number added is stored in |entries|. The return value is
  // the time until the next closure is ready. Is is
  // std::numeric_limits<int>::max() if there are no closures scheduled.
  std::chrono::steady_clock::time_point GetReadyClosures(
      ClosureList* to_run, int* entries, std::chrono::steady_clock::time_point now);

  Mutex lock_;
  ClosureList closures_;
  int num_entries_ = 0;
  int head_ = 0;
};

}  // namespace esp_cxx

#endif  // ESPCXX_EVENT_MANAGER_H_
