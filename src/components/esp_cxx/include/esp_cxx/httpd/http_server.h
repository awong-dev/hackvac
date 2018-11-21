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

  class Endpoint {
   public:
    virtual ~Endpoint() = default;
    virtual void OnHttp(const HttpRequest& request, bool is_multipart, HttpResponse response) {}
    virtual void OnMultipart(HttpMultipart* multipart, HttpResponse response) {}

    static void OnHttpEventThunk(mg_connection *nc, int event,
                                 void *ev_data, void *user_data);
  };

  // Starts the server.
  void Start();

  // Adds an Endpoint handler for the given path_pattern.
  void RegisterEndpoint(const char* path_pattern, Endpoint* endpoint);

  // Useful for disabling parts of the endpoint handlers.
  static void IgnoreHttp(const HttpResponse&, HttpResponse) {}
  static void IgnoreMultipart(HttpMultipart*, HttpResponse) {}

  // Conveience wrapper for adding endpoint handlers using a bare function.
  typedef void (*HttpCallback)(const HttpRequest& request, bool is_multipart, HttpResponse response);
  typedef void (*HttpMultipartCallback)(HttpMultipart* multipart, HttpResponse response);
  // TODO(awong): Create a do-nothing handler instead of using nullptr.
  template <HttpCallback handler, HttpMultipartCallback multipart_cb = &IgnoreMultipart>
  void RegisterEndpoint(const char* path_pattern) {
    static struct : Endpoint {
      void OnHttp(const HttpRequest& request, bool is_multipart, HttpResponse response) {
        // TODO(awong): rename handler to be consistent with multipart_cb here and elsewhere.
        return handler(request, is_multipart, std::move(response));
      }
      void OnMultipart(HttpMultipart* multipart, HttpResponse response) {
        return multipart_cb(multipart, std::move(response));
      }
    } endpoint;
    RegisterEndpoint(path_pattern, &endpoint);
  }

 private:
  // Thunk for executing the actual run loop.
  static void EventPumpThunk(void* parameters);

  // Pumps events for the http server.
  void EventPumpRunLoop();

  // Default event handler for the bound port. Run if no other handler
  // intercepts first.
  static void DefaultHandlerThunk(struct mg_connection *nc,
                                  int event,
                                  void *event_data,
                                  void *user_data);

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
