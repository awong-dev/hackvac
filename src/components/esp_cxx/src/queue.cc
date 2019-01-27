#include "esp_cxx/queue.h"

#include "esp_cxx/task.h"

#ifdef FAKE_ESP_IDF
#include <unistd.h>
#include <chrono>
#include <numeric>

namespace {

std::chrono::time_point<std::chrono::steady_clock> ToAbsTime(int rel_time_ms) {
  return std::chrono::steady_clock::now() + std::chrono::milliseconds(rel_time_ms);
}

}  // namespace
#endif

namespace esp_cxx {

Queue::Queue() = default;

#ifndef FAKE_ESP_IDF
Queue::Queue(int num_elements, size_t element_size) 
  : queue_(xQueueCreate(num_elements, sizeof(ElementType))) {
}
#else
Queue::Queue(int num_elements, size_t element_size) 
  : queue_(new std::queue<ElementType>()),
    max_items_(num_elements) {
}
#endif

Queue::~Queue() {
#ifndef FAKE_ESP_IDF
  if (queue_) {
    vQueueDelete(queue_);
  }
#else
#endif
}

bool Queue::Push(const ElementType obj, int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return xQueueSend(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  std::unique_lock<std::mutex> lock(lock_);
  auto abs_timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (queue_->size() >= max_items_) {
    // Wait until a dequeue, but fall back into the wait if someone raced an push.
    if (on_pop_.wait_until(lock, abs_timeout) == std::cv_status::timeout) {
      return false;
    }
  }

  // If here, there is space in the queue.
  queue_->push(obj);
  on_push_.notify_one();

  return true;
#endif
}

bool Queue::Peek(ElementType obj, int timeout_ms) const {
#ifndef FAKE_ESP_IDF
  return xQueuePeek(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  std::unique_lock<std::mutex> lock(lock_);
  auto abs_timeout = ToAbsTime(timeout_ms);
  while (queue_->empty()) {
    // Wait until a dequeue, but fall back into the wait if someone raced an push.
    if (on_push_.wait_until(lock, abs_timeout) == std::cv_status::timeout) {
      return false;
    }
  }

  // If here, there is an element.
  obj = queue_->front();

  return true;
#endif
}

bool Queue::Pop(void* obj, int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return xQueueReceive(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  std::unique_lock<std::mutex> lock(lock_);
  auto abs_timeout = ToAbsTime(timeout_ms);
  while (queue_->empty()) {
    // Wait until a dequeue, but fall back into the wait if someone raced an push.
    if (on_push_.wait_until(lock, abs_timeout) == std::cv_status::timeout) {
      return false;
    }
  }

  // If here, there is an element.
  obj = queue_->front();
  queue_->pop();
  on_pop_.notify_one();

  return true;
#endif
}

#ifndef FAKE_ESP_IDF
QueueSet::QueueSet(int max_items) 
  : queue_set_(xQueueCreateSet(max_items)) {
}
#else
QueueSet::QueueSet(int max_items) {
  // max_items unused. Too hard to emulate.
  FD_ZERO(&queue_fds_);
}
#endif

QueueSet::~QueueSet() {
#ifndef FAKE_ESP_IDF
  vQueueDelete(queue_set_);
#endif
}

void QueueSet::Add(Queue* queue) {
#ifndef FAKE_ESP_IDF
  xQueueAddToSet(queue->queue_, queue_set_);
#else
  int fildes[2];
  int ret = pipe(&fildes[0]);
  assert(ret == 0);
  queue->queueset_fd_ = fildes[0];
  FD_SET(fildes[1], &queue_fds_);
  pipe_pairs_[fildes[0]] = fildes[1];
#endif
}

void QueueSet::Remove(Queue* queue) {
#ifndef FAKE_ESP_IDF
  xQueueRemoveFromSet(queue->queue_, queue_set_);
#else
  int fd = queue->queueset_fd_;
  queue->queueset_fd_ = -1;

  int ret = close(fd);
  assert(ret == 0);

  int fd2 = pipe_pairs_[fd];
  FD_CLR(fd2, &queue_fds_);
  pipe_pairs_.erase(fd2);
  ret = close(fd2);
  assert(ret == 0);
#endif
}

Queue::Id QueueSet::Select(int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return reinterpret_cast<Queue::Id>(
      xQueueSelectFromSet(queue_set_, timeout_ms / portTICK_PERIOD_MS));
#else
  struct timeval tv = {
    timeout_ms / 1000,
    (timeout_ms % 1000) * 1000,
  };
  int nfds = 0;
  for (auto item : pipe_pairs_) {
    nfds = std::max(nfds, item.second);
  }
  return select(nfds, &queue_fds_, nullptr, nullptr, &tv);
#endif
}

}  // namespace esp_cxx
