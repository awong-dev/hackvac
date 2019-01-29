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

void MongooseEventManager::Poll(int timeout_ms) {
  mg_mgr_poll(underlying_manager(), timeout_ms);
}

void MongooseEventManager::Wake() {
  mg_broadcast(underlying_manager(), &DoNothing, nullptr, 0);
}

}  // namespace esp_cxx

