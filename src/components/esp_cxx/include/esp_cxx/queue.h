#ifndef ESPCXX_QUEUE_H_
#define ESPCXX_QUEUE_H_

#include <cstdint>
#include <utility>
#include <limits>

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include <unistd.h>

#include <map>
#include <mutex>
#include <queue>
#endif

namespace esp_cxx {

class Queue {
 public:
  using Id = intptr_t;
  using ElementType = void*;
  // TODO(awong): Fix to be actual max. And make sure to stop the hokey kScaleDelay
  // in queue.cc otherwise this will infinite loop.
  static constexpr int kMaxWait = 99999;

  Queue();
  Queue(int num_elements, size_t element_size);
  ~Queue();

#ifndef FAKE_ESP_IDF
  // Takes ownership of an externally created FreeRTOS queue.
  explicit Queue(QueueHandle_t queue) : queue_(queue) {}
#endif

  bool Push(const ElementType obj, int timeout_ms = 0);
  bool Peek(ElementType obj, int timeout_ms = 0) const;
  bool Pop(ElementType obj, int timeout_ms = 0);
  bool IsId(Id id) const { return id == queueset_fd_; }
  Id id() const { return queueset_fd_; }

  decltype(auto) underlying() const { return queue_; }

 private:
#ifndef FAKE_ESP_IDF
  QueueHandle_t queue_ = nullptr;
#else
  friend class QueueSet;

  mutable std::mutex lock_{};
  mutable std::condition_variable on_push_{};
  std::condition_variable on_pop_{};

  std::queue<ElementType>* queue_;
  int max_items_ = 0;

  int queueset_fd_ = -1;  // Wake signal.
#endif
};

class QueueSet {
 public:
  // max_items should be the great or equal to the total number of elements
  // of all queues that are added to the set.
  explicit QueueSet(int max_items);
  ~QueueSet();

  void Add(Queue* queue);
  void Remove(Queue* queue);

  Queue::Id Select(int timeout_ms);

 private:
#ifndef FAKE_ESP_IDF
  QueueSetHandle_t queue_set_ = nullptr;
#else
  std::map<int, int> pipe_pairs_;
  fd_set queue_fds_;
#endif
};

}  // namespace esp_cxx

#endif  // ESPCXX_QUEUE_H_

