#include "esp_cxx/httpd/event_manager.h"

#include <algorithm>
#include <mutex>

namespace {
void DoNothing(mg_connection* nc, int ev, void* ev_data, void* user_data) {
}
}  // namespace

namespace esp_cxx {
EventManager::EventManager() {
  mg_mgr_init(&event_manager_, this);
}

void EventManager::Add(std::function<void(void)> closure) {
  AddDelayed(std::move(closure), 0);
}

void EventManager::AddDelayed(std::function<void(void)> closure, int milliseconds) {
  int now = 0; // TODO(awong): Get real time.
  {
    std::lock_guard<Mutex> lock(lock_);
    while (num_entries_ >= closures_.size()) {
      // TODO(awong): Wait until there's space or drop?
    }

    num_entries_++;
    closures_[(head_ + num_entries_) % closures_.size()] = {std::move(closure), now + milliseconds};

    // Wake up the poll loop.
    mg_broadcast(&event_manager_, &DoNothing, nullptr, 0);
  }
}

void EventManager::Run() {
  int next_wake = std::numeric_limits<int>::max();

  for (;;) {
    mg_mgr_poll(&event_manager_, next_wake);
    ClosureList to_run;
    int num_to_run = 0;
    int now = 0; // TODO(awong): Actually call now.
    next_wake = GetReadyClosures(&closures_, &num_to_run, now);
    for (size_t i = 0; i < num_to_run; i++) {
      closures_[i].thunk();
    }
  }
}

int EventManager::GetReadyClosures(ClosureList* to_run, int* entries, int now) {
  int next_wake = std::numeric_limits<int>::max();
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

