#ifndef ESPCXX_RINGBUFFER_H_
#define ESPCXX_RINGBUFFER_H_

#include <atomic>

#include "esp_cxx/mutex.h"

namespace esp_cxx {

// Typesafe, locked, ringbuffer. Suitable for use in communicating between
// two tasks. Particularly useful for passing around std::unique_ptr<> as
// discarded elements will be returned back to the caller as temporary which
// can then trigger normal RAII clean-up.
template <typename T, size_t size>
class RingBuffer {
 public:
  uint32_t dropped_elements() const { return dropped_elements_; }
  // Adds |obj| into the RingBuffer. If this overwrites 
  T Put(T&& obj) {
    AutoMutex lock(&mutex_);

    swap(data_[queue_end_], obj);
    queue_end_ = (queue_end_ + 1) % size;

    // If num_items_ == size, then this is an overwrite, not
    // an add.
    if (num_items_ < size) {
      num_items_++;
    } else {
      dropped_elements_++;
    }

    return std::move(obj);
  }

  bool Get(T* obj) {
    AutoMutex lock(&mutex_);

    if (num_items_ == 0) {
      return false;
    }

    size_t offset = queue_end_ - num_items_;
    // The head is larger than the tail. Queue is wrapped.
    if (offset < 0) {
      offset = size - offset;
    }

    *obj = std::move(data_[offset]);

    num_items_--;
    return true;
  }

 private:
  Mutex mutex_;

  // Number of elements dropped from this queue.
  std::atomic_uint_fast32_t dropped_elements_{0};

  // Position to insert the next element.
  size_t queue_end_ = 0;
  
  // Current number of items in the queue.
  size_t num_items_ = 0;

  // Actual data inside the queue.
  std::array<T, size> data_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_RINGBUFFER_H_

