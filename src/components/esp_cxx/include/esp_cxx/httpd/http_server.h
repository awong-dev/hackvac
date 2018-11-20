#ifndef ESPCXX_HTTPD_HTTP_SERVER_H_
#define ESPCXX_HTTPD_HTTP_SERVER_H_

#include "mongoose.h"

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"

namespace esp_cxx {

class HttpServer {
 public:
  HttpServer(const char* name, const char* port);
  ~HttpServer();

  // Starts the server.
  void Start();

  // Adds a handler for the given path_pattern.
  //
  // Mongoose is really annoying in that it doesn't allow userdata per
  // endpoint without invasively changing its API. Any state needed should
  // be either added to the HttpServer as a context.
  //
  // TODO(awong): Add some sort of type-based user-context to the HttpServer
  // as a way to hack inheritance.
  void AddEndpoint(const char* path_pattern,
                   void (*handler)(mg_connection*, int event, void* ev_data));

  typedef void (*EndPointCallback)(const HttpRequest& request, HttpResponse response);

  template <EndPointCallback handler>
  void RegisterEndpoint(const char* path_pattern) {
    AddEndpoint(path_pattern, &CxxHandlerAdaptor<handler>);
  }

 private:
  // Thunk for executing the actual run loop.
  static void EventPumpThunk(void* parameters);

  template <EndPointCallback handler>
  static void CxxHandlerAdaptor(mg_connection* new_connection, int event, void* ev_data) {
    CxxHandlerWrapper(new_connection, event, ev_data, handler);
  }

  static void CxxHandlerWrapper(mg_connection* new_connection, int event, void* ev_data,
                                EndPointCallback callback);

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

#endif  // ESPCXX_HTTPD_HTTP_SERVER_H_
