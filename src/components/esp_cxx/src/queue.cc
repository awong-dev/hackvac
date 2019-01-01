#include "esp_cxx/queue.h"

#include "esp_cxx/task.h"

namespace esp_cxx {

Queue::Queue() = default;

#ifndef FAKE_ESP_IDF
Queue::Queue(int num_elements, size_t element_size) 
  : queue_(xQueueCreate(num_elements, element_size)) {
}
#else
Queue::Queue(int num_elements, size_t element_size) 
  : queue_(new std::queue<void*>()) {
}
#endif

Queue::~Queue() {
#ifndef FAKE_ESP_IDF
  if (queue_) {
    vQueueDelete(queue_);
  }
#endif
}

bool Queue::Push(const void* obj, int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return xQueueSend(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  Task::Delay(timeout_ms * 1000); // TODO(awong): Fix.
  return false;
#endif
}

bool Queue::Peek(void* obj, int timeout_ms) const {
#ifndef FAKE_ESP_IDF
  return xQueuePeek(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  Task::Delay(timeout_ms * 1000); // TODO(awong): Fix.
  return false;
#endif
}

bool Queue::Pop(void* obj, int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return xQueueReceive(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
#else
  Task::Delay(timeout_ms);
  return false;
#endif
}

#ifndef FAKE_ESP_IDF
QueueSet::QueueSet(int max_items) 
  : queue_set_(xQueueCreateSet(max_items)) {
}
#else
QueueSet::QueueSet(int max_items) {
}
#endif

QueueSet::~QueueSet() {
#ifndef FAKE_ESP_IDF
  vQueueDelete(queue_set_);
#endif
}

void QueueSet::Add(Queue* queue) {
#ifndef FAKE_ESP_IDF
  xQueueAddToSet(queue, queue_set_);
#endif
}

Queue::Id QueueSet::Select(int timeout_ms) {
#ifndef FAKE_ESP_IDF
  return reinterpret_cast<Queue::Id>(
      xQueueSelectFromSet(queue_set_, timeout_ms / portTICK_PERIOD_MS));
#else
  Task::Delay(timeout_ms * 1000); // TODO(awong): Fix.
  return {};
#endif
}

}  // namespace esp_cxx
