#include "esp_cxx/queue.h"

#include "esp_cxx/task.h"

#ifdef FAKE_ESP_IDF

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>

namespace {

std::chrono::time_point<std::chrono::steady_clock> ToAbsTime(int rel_time_ms) {
  return std::chrono::steady_clock::now() + std::chrono::milliseconds(rel_time_ms);
}

}  // namespace
#endif

namespace esp_cxx {

QueueBase::QueueBase() = default;

#ifndef FAKE_ESP_IDF
QueueBase::QueueBase(int num_elements, size_t element_size) 
  : queue_(xQueueCreate(num_elements, element_size)) {
}
#else
QueueBase::QueueBase(int num_elements, size_t element_size) 
  : max_items_(num_elements), element_size_(element_size) {
}
#endif

QueueBase::~QueueBase() {
#ifndef FAKE_ESP_IDF
  if (queue_) {
    vQueueDelete(queue_);
  }
#else
#endif
}

bool QueueBase::RawPush(const void* obj, int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return xQueueSend(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  std::unique_lock<std::mutex> lock(lock_);
  auto abs_timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (queue_.size() >= max_items_) {
    // Wait until a dequeue, but fall back into the wait if someone raced an push.
    if (on_pop_.wait_until(lock, abs_timeout) == std::cv_status::timeout) {
      return false;
    }
  }

  // If here, there is space in the queue.
  std::unique_ptr<char[]> buf(new char[element_size_]);
  memcpy(&buf[0], obj, element_size_);
  queue_.push(std::move(buf));
  on_push_.notify_one();

  if (queueset_fd_ != QueueBase::kInvalidId) {
    while(write(queueset_fd_, "", 1) == -EINTR);
  }

  return true;
#endif
}

bool QueueBase::RawPeek(void* obj, int timeout_ms) const {
#ifndef FAKE_ESP_IDF
  return xQueuePeek(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  std::unique_lock<std::mutex> lock(lock_);
  auto abs_timeout = ToAbsTime(timeout_ms);
  while (queue_.empty()) {
    // Wait until a dequeue, but fall back into the wait if someone raced an push.
    if (on_push_.wait_until(lock, abs_timeout) == std::cv_status::timeout) {
      return false;
    }
  }

  // If here, there is an element.
  memcpy(obj, &queue_.front()[0], element_size_);

  return true;
#endif
}

bool QueueBase::RawPop(void* obj, int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return xQueueReceive(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  std::unique_lock<std::mutex> lock(lock_);
  auto abs_timeout = ToAbsTime(timeout_ms);
  while (queue_.empty()) {
    // Wait until a dequeue, but fall back into the wait if someone raced an push.
    if (on_push_.wait_until(lock, abs_timeout) == std::cv_status::timeout) {
      return false;
    }
  }

  // If here, there is an element.
  memcpy(obj, &queue_.front()[0], element_size_);
  queue_.pop();
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
}
#endif

QueueSet::~QueueSet() {
#ifndef FAKE_ESP_IDF
  vQueueDelete(queue_set_);
#endif
}

void QueueSet::Add(QueueBase* queue) {
#ifndef FAKE_ESP_IDF
  xQueueAddToSet(queue->queue_, queue_set_);
#else
  int fildes[2];
  int ret = pipe(&fildes[0]);
  assert(ret == 0);
  queue->queueset_fd_ = fildes[1];
  pipe_pairs_[fildes[1]] = fildes[0];
#endif
}

void QueueSet::Remove(QueueBase* queue) {
#ifndef FAKE_ESP_IDF
  xQueueRemoveFromSet(queue->queue_, queue_set_);
#else
  int fd = queue->queueset_fd_;
  queue->queueset_fd_ = QueueBase::kInvalidId;

  int ret = close(fd);
  assert(ret == 0);

  int fd2 = pipe_pairs_[fd];
  pipe_pairs_.erase(fd);
  ret = close(fd2);
  assert(ret == 0);
#endif
}

QueueBase::Id QueueSet::Select(int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return reinterpret_cast<QueueBase::Id>(
      xQueueSelectFromSet(queue_set_, timeout_ms / portTICK_PERIOD_MS));
#else
  struct timeval tv = {
    timeout_ms / 1000,
    (timeout_ms % 1000) * 1000,
  };
  int nfds = 0;
  fd_set read_fds;
  FD_ZERO(&read_fds);
  for (auto item : pipe_pairs_) {
    nfds = std::max(nfds, item.second);
    FD_SET(item.second, &read_fds);
  }
  nfds++; // select() requires 1 past max.
  int num_ready = 0;
  while ((num_ready = select(nfds, &read_fds, nullptr, nullptr, &tv)) == -EINTR);

  if (num_ready > 0) {
    int *fds = static_cast<int*>(alloca(num_ready * sizeof(int)));
    int cur = 0;
    for (auto item : pipe_pairs_) {
      if (FD_ISSET(item.second, &read_fds)) {
        fds[cur++] = item.second;
      }
    }

    // Shuffle the ready file descriptors so a really busy FD cannot
    // starve the others.
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(fds, fds + num_ready, g);
    return fds[0];
  }
  return QueueBase::kInvalidId;
#endif
}

}  // namespace esp_cxx
