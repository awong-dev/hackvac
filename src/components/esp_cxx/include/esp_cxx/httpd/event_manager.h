#ifndef ESPCXX_HTTPD_EVENT_MANAGER_H_
#define ESPCXX_HTTPD_EVENT_MANAGER_H_

#include <array>
#include <functional>

#include "esp_cxx/mutex.h"
#include "mongoose.h"

namespace esp_cxx {

class EventManager {
 public:
  EventManager();

  // Will run |task| as soon as possible.
  void Add(std::function<void(void)> closure);

  // Will run |task| at least milliseconds after this is called.
  void AddDelayed(std::function<void(void)> closure, int milliseconds);

  // Waits for next I/O event or task before waking up.
  void Run();

  mg_mgr* underlying_manager() { return &event_manager_; }

 private:
  struct ClosureEntry {
    std::function<void(void)> thunk;
    int run_at = 0;
    bool operator<(const ClosureEntry& other) const { return run_at < other.run_at; }
  };
  using ClosureList = std::array<ClosureEntry, 10>;

  mg_mgr event_manager_;

  // Places all closures that are scheduled before |now| in to |to_run|.
  // The number added is stored in |entries|. The return value is
  // the time until the next closure is ready. Is is
  // std::numeric_limits<int>::max() if there are no closures scheduled.
  int GetReadyClosures(ClosureList* to_run, int* entries, int now);

  Mutex lock_;
  ClosureList closures_;
  int num_entries_ = 0;
  int head_ = 0;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_EVENT_MANAGER_H_
