#include "esp_cxx/httpd/websocket.h"

namespace esp_cxx {
class EventManager {
 public:
  EventManager();

  // Waits |milliseconds| for an I/O event. Returns number of events
  // available.
  // TODO(awong): Should use int::max()
  int Poll(int milliseconds = 1000000);

  mg_mgr* underlying_manager() { return &event_manager_; }

 private:
  mg_mgr event_manager_;
};

}  // namespace esp_cxx
