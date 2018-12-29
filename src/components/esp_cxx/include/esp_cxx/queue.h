#ifndef ESPCXX_QUEUE_H_
#define ESPCXX_QUEUE_H_

#include <cstdint>
#include <utility>

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include <queue>
#endif

namespace esp_cxx {

class Queue {
 public:
  using Id = intptr_t;
  static constexpr int kMaxWait = -1; // TODO(awong): Fix to be actual max.

  Queue();
  Queue(int num_elements, size_t element_size);
  ~Queue();

#ifndef FAKE_ESP_IDF
  // Takes ownership of an externally created FreeRTOS queue.
  explicit Queue(QueueHandle_t queue) : queue_(queue) {}
#endif

  bool Push(const void* obj, int timeout_ms = 0);
  bool Peek(void* obj, int timeout_ms = 0) const;
  bool Pop(void* obj, int timeout_ms = 0);
  bool IsId(Id id) const { return id == reinterpret_cast<Id>(queue_); }

  decltype(auto) underlying() const { return queue_; }

 private:
#ifndef FAKE_ESP_IDF
  QueueHandle_t queue_ = nullptr;
#else
  // Use a DataBuffer?
  std::queue<void*>* queue_;
#endif
};

class QueueSet {
 public:
  // max_items should be the great or equal to the total number of elements
  // of all queues that are added to the set.
  explicit QueueSet(int max_items);
  ~QueueSet();

  void Add(Queue* queue);

  Queue::Id Select(int timeout_ms);

 private:
#ifndef FAKE_ESP_IDF
  QueueSetHandle_t queue_set_ = nullptr;
#else
#endif
};

}  // namespace esp_cxx

#endif  // ESPCXX_QUEUE_H_

