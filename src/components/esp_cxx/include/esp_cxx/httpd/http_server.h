#ifndef ESPCXX_HTTP_HTTP_SERVER_H_
#define ESPCXX_HTTP_HTTP_SERVER_H_

#include "mongoose.h"

namespace esp_cxx {

class HttpServer {
 public:
  HttpServer(const char* name, const char* port);
  ~HttpServer();

  // Starts the server.
  void Start();

 private:
  // Thunk for executing the actual run loop.
  static void EventPumpThunk(void* parameters);

  // Pumps events for the http server.
  void EventPumpRunLoop();

  // Default event handler for the bound port. Run if no other handler
  // intercepts first.
  static void DefaultHandlerThunk(struct mg_connection *nc,
                                  int event,
                                  void *eventData);

  // Name for pump task.
  const char* name_;

  // Port to connect to. Usually ":80" is good.
  const char* port_;

  // Bound connection to |port_|.
  mg_connection* connection_;

  // Event manager for all connections on this HTTP server.
  mg_mgr event_manager_;

  // Handle of event pump task.
  TaskHandle_t pump_task_ = nullptr;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTP_HTTP_SERVER_H_
