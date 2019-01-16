#include "esp_cxx/httpd/event_manager.h"

namespace esp_cxx {

EventManager::EventManager() {
  mg_mgr_init(&event_manager_, this);
}

int EventManager::Poll(int timeout_ms) {
  return mg_mgr_poll(&event_manager_, timeout_ms);
}

}  // namespace esp_cxx

