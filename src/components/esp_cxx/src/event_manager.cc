#include "esp_cxx/event_manager.h"

#include <algorithm>
#include <mutex>

namespace {
void DoNothing(mg_connection* nc, int ev, void* ev_data, void* user_data) {
}
}  // namespace

namespace esp_cxx {

void EventManager::Add(std::function<void(void)> closure) {
  AddDelayed(std::move(closure), 0);
}

void EventManager::AddDelayed(std::function<void(void)> closure, int delay_ms) {
  auto run_at = std::chrono::steady_clock::now() +  std::chrono::milliseconds(delay_ms);
  {
    std::lock_guard<Mutex> lock(lock_);
    while (num_entries_ >= closures_.size()) {
      // TODO(awong): Wait until there's space or drop? We need a cv.
    }

    num_entries_++;
    closures_[(head_ + num_entries_) % closures_.size()] = {std::move(closure), run_at};

    // Wake up the poll loop.
    Wake();
  }
}

void EventManager::Run() {
   TimePoint next_wake = TimePoint::max();

  for (;;) {
    Poll(next_wake - std::chrono::steady_clock::now());
    ClosureList to_run;
    int num_to_run = 0;
    next_wake = GetReadyClosures(&closures_, &num_to_run, std::chrono::steady_clock::now());
    for (size_t i = 0; i < num_to_run; i++) {
      closures_[i].thunk();
    }
  }
}

EventManager::TimePoint EventManager::GetReadyClosures(ClosureList* to_run, int* entries,
                                   std::chrono::steady_clock::time_point now) {
  TimePoint next_wake = TimePoint::max();
  *entries = 0;
  {
    std::lock_guard<Mutex> lock(lock_);
    int last_inserted = 0;
    for (int i = 0; i < num_entries_; i++) {
      if (closures_[i].run_at < now) {
        (*to_run)[(*entries)++] = std::move(closures_[i]);
      } else {
        next_wake = std::min(closures_[i].run_at, next_wake);
        closures_[last_inserted++] = std::move(closures_[i]);
      }
    }
  }

  // Sort the to_run list.
  std::make_heap(to_run->begin(), to_run->begin() + *entries);
  std::sort_heap(to_run->begin(), to_run->begin() + *entries);
  return next_wake;
}

}  // namespace esp_cxx

