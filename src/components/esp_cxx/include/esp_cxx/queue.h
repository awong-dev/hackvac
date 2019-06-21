#ifndef ESPCXX_QUEUE_H_
#define ESPCXX_QUEUE_H_

#include <cstdint>
#include <limits>
#include <utility>

#include "esp_cxx/cxx17hack.h"

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include <unistd.h>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#endif

namespace esp_cxx {

class QueueBase {
 public:
  using Id = intptr_t;
  constexpr static int kInvalidId = -1;

  QueueBase();  // Needed by UART API since it initializes the queue late.
  QueueBase(int num_elements, size_t element_size);

#ifndef FAKE_ESP_IDF
  // Takes ownership of an externally created FreeRTOS queue.
  explicit QueueBase(QueueHandle_t queue) : queue_(queue) {}
#endif

  bool IsId(Id id) const { return id == queueset_fd_; }
  Id id() const { return queueset_fd_; }

 protected:
  ~QueueBase();

  bool RawPush(const void* obj, int timeout_ms = 0);
  bool RawPeek(void* obj, int timeout_ms = 0) const;
  bool RawPop(void* obj, int timeout_ms = 0);

 private:
#ifndef FAKE_ESP_IDF
  QueueHandle_t queue_ = nullptr;
#else
  friend class QueueSet;

  mutable std::mutex lock_{};
  mutable std::condition_variable on_push_{};
  std::condition_variable on_pop_{};

  std::queue<std::unique_ptr<char[]>> queue_;
  int max_items_ = 0;
  int element_size_ = 0;

  int queueset_fd_ = -1;  // Wake signal.
#endif
};

template <typename T>
class Queue : public QueueBase {
 public:
  explicit Queue(int max_items)
    : QueueBase(max_items, sizeof(T)) {}

#ifndef FAKE_ESP_IDF
  // Takes ownership of an externally created FreeRTOS queue.
  explicit Queue(QueueHandle_t queue) : QueueBase(queue) {}
#endif

  bool Push(const T& obj, int timeout_ms = 0) {
    return RawPush(reinterpret_cast<const void*>(&obj), timeout_ms);
  }

  bool Peek(T* obj, int timeout_ms = 0) {
    return RawPeek(obj, timeout_ms);
  }

  bool Pop(T* obj, int timeout_ms = 0) {
    return RawPop(obj, timeout_ms);
  }

  // Used to create a queue of 0 size for APIs, like UART, that initialize the
  // queue later.
  static Queue CreateNullQueue() { return Queue(); }

 private:
  Queue() = default;
};

class QueueSet {
 public:
  // max_items should be the great or equal to the total number of elements
  // of all queues that are added to the set.
  explicit QueueSet(int max_items);
  ~QueueSet();

  void Add(QueueBase* queue);
  void Remove(QueueBase* queue);

  QueueBase::Id Select(int timeout_ms);

 private:
#ifndef FAKE_ESP_IDF
  QueueSetHandle_t queue_set_ = nullptr;
#else
  std::map<int, int> pipe_pairs_;
#endif
};

}  // namespace esp_cxx

#endif  // ESPCXX_QUEUE_H_

