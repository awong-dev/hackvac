#ifndef ESPCXX_HTTPD_LOG_STREAM_ENDPOINT_H_
#define ESPCXX_HTTPD_LOG_STREAM_ENDPOINT_H_

#include "esp_cxx/httpd/http_server.h"

#include <atomic>

namespace esp_cxx {

class LogStreamEndpoint : public HttpServer::Endpoint {
 public:
  virtual void OnWebsocketHandshake(HttpRequest request, HttpResponse response);
  virtual void OnWebsocketHandshakeComplete(WebsocketSender sender);
  virtual void OnWebsocketFrame(WebsocketFrame frame, WebsocketSender sender);
  virtual void OnWebsocketClosed(WebsocketSender sender);

  void PublishLog(std::string_view log);

 private:
  // Only set if there is some active websocket connection.
  std::atomic<mg_mgr*> event_manager_{nullptr};

  // Simple avoidance of DoS.
  static constexpr int kMaxListeners = 5;
  size_t num_listeners_ = 0;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_LOG_STREAM_ENDPOINT_H_


