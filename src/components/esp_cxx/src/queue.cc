#include "esp_cxx/queue.h"
#if 0

namespace esp_cxx {

Queue::Queue() = default;

Queue::Queue(int num_elements, size_t element_size) 
  : queue_(xQueueCreate(num_elements, element_size)) {
}

Queue::~Queue() {
  if (queue_) {
    vQueueDelete(queue_);
  }
}

bool Queue::Push(const void* obj, int timeout_ms) {
  return xQueueSend(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
}

bool Queue::Peek(void* obj, int timeout_ms) const {
  return xQueuePeek(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
}

bool Queue::Pop(void* obj, int timeout_ms) {
  return xQueueReceive(queue_, obj, timeout_ms / portTICK_PERIOD_MS) == pdTRUE;
}

QueueSet::QueueSet(int max_items) 
  : queue_set_(xQueueCreateSet(max_items)) {
}

QueueSet::~QueueSet() {
  vQueueDelete(queue_set_);
}

void QueueSet::Add(Queue* queue) {
  xQueueAddToSet(queue, queue_set_);
}

Queue::Id QueueSet::Select(int timeout_ms) {
  return reinterpret_cast<Queue::Id>(
      xQueueSelectFromSet(queue_set_, timeout_ms / portTICK_PERIOD_MS));
}

}  // namespace esp_cxx
#endif
