#include "esp_cxx/event_manager.h"

#include <algorithm>
#include <mutex>

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
    // Do the poll.
    auto timeout_ms = next_wake - std::chrono::steady_clock::now();
    auto raw_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout_ms).count();
    int actual_timeout_ms = std::numeric_limits<int>::max();
    // Saturate at max int.
    if (raw_ms < actual_timeout_ms) {
      actual_timeout_ms = std::min(0, static_cast<int>(raw_ms));
    }
    Poll(actual_timeout_ms);

    // Run the closures.
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

// Initialize to 1 more than max events to allow for the Wake() call.
QueueSetEventManager::QueueSetEventManager(int max_waiting_events)
  : underlying_queue_set_(max_waiting_events + 1) {
}

void QueueSetEventManager::Add(Queue* queue,
                               std::function<void(void)> on_data_cb) {
  underlying_queue_set_.Add(queue);
  callbacks_[queue->id()] = std::move(on_data_cb);
}

void QueueSetEventManager::Remove(Queue* queue) {
  underlying_queue_set_.Remove(queue);
  callbacks_.erase(queue->id());
}

void QueueSetEventManager::Poll(int timeout_ms) {
  Queue::Id id = underlying_queue_set_.Select(timeout_ms);
  callbacks_[id]();
}

void QueueSetEventManager::Wake() {
  // TODO(awong): There should be a semaphore that we can signal.
}

}  // namespace esp_cxx

