#include "esp_cxx/httpd/mongoose_event_manager.h"

#include <chrono>

#include "esp_cxx/event_manager.h"

namespace {
void DoNothing(mg_connection* nc, int ev, void* ev_data, void* user_data) {
}
}  // namespace

namespace esp_cxx {
MongooseEventManager::MongooseEventManager() {
  mg_mgr_init(&underlying_manager_, this);
}

MongooseEventManager::~MongooseEventManager() {
}

void MongooseEventManager::Poll(Duration timeout_ms) {
  auto raw_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout_ms).count();
  int actual_timeout_ms = std::numeric_limits<int>::max();
  // Saturate at max int.
  if (raw_ms < actual_timeout_ms) {
    actual_timeout_ms = std::min(0, static_cast<int>(raw_ms));
  }
  mg_mgr_poll(underlying_manager(), actual_timeout_ms);
}

void MongooseEventManager::Wake() {
  mg_broadcast(underlying_manager(), &DoNothing, nullptr, 0);
}

}  // namespace esp_cxx

