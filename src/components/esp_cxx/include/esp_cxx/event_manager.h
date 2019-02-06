#ifndef ESPCXX_EVENT_MANAGER_H_
#define ESPCXX_EVENT_MANAGER_H_

#include <array>
#include <functional>
#include <unordered_map>

#include "esp_cxx/mutex.h"
#include "esp_cxx/queue.h"
#include "mongoose.h"

namespace esp_cxx {

class EventManager {
 public:
  using Duration = std::chrono::steady_clock::duration;
  using TimePoint = std::chrono::steady_clock::time_point;

  // Will run |task| as soon as possible.
  void Run(std::function<void(void)> closure);

  // Will run |task| at least milliseconds after this is called.
  void RunDelayed(std::function<void(void)> closure, int milliseconds);

  // Will run |task| on or after |run_after|.  If |run_after| is in the past,
  // closure will execute as soon as the event loop is free. It is possible
  // to starve a task if callers keeps passing |run_after| at earlier time
  // points. Don't do that.
  void RunAfter(std::function<void(void)> closure, TimePoint run_after);

  // Continually polls for next I/O event or task.
  void Loop();

  // Make Loop() above return.
  void Quit();

 protected:
  EventManager() = default;
  virtual ~EventManager() = default;

  virtual void Poll(int timeout_ms) = 0;
  virtual void Wake() = 0;

 private:
  struct ClosureEntry {
    std::function<void(void)> thunk;
    TimePoint run_after = TimePoint::min();
    bool operator<(const ClosureEntry& other) const { return run_after < other.run_after; }
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
  bool has_quit_ = false;
};

class QueueSetEventManager : public EventManager {
 public:
  // Initialize a event manager that waits on a QueueSet. The
  // |max_waiting_events| size is the maximum queue length before data
  // is dropped. It should be calculated based on the total size of all
  // queues added to thie undelrying queueset.
  //
  // Note this number is NOT the number of cloures that are scheduled
  // for the manager. That is a separate set of storge.
  explicit QueueSetEventManager(int max_waiting_events);

  void Add(QueueBase* queue, std::function<void(void)> on_data_cb);
  void Remove(QueueBase* queue);

  QueueSet* underlying_queue_set() { return &underlying_queue_set_; }

 protected:
  void Poll(int timeout_ms) override;
  void Wake() override;

 private:
  // TODO(awong): Using an unordered_map here is overkill. Unsorted array
  // is probably just fine.
  std::unordered_map<QueueBase::Id, std::function<void(void)>> callbacks_;
  QueueSet underlying_queue_set_;

#ifndef FAKE_ESP_IDF
#error Write semaphore code here.
#else
  esp_cxx::Queue<char> wake_queue_{1};
#endif
};

}  // namespace esp_cxx

#endif  // ESPCXX_EVENT_MANAGER_H_
