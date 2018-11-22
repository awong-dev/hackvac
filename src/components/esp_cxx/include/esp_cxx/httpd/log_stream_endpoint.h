#ifndef ESPCXX_HTTPD_LOG_STREAM_ENDPOINT_H_
#define ESPCXX_HTTPD_LOG_STREAM_ENDPOINT_H_

#include "esp_cxx/httpd/http_server.h"

#include <array>

namespace esp_cxx {

class LogStreamEndpoint : public HttpServer::Endpoint {
 public:
  virtual void OnWebsocketHandshake(HttpRequest request, HttpResponse response);
  virtual void OnWebsocketHandshakeComplete(WebsocketSender sender);
  virtual void OnWebsocketFrame(WebsocketFrame frame, WebsocketSender sender);
  virtual void OnWebsocketClosed(WebsocketSender sender);

 private:
  static constexpr int kMaxListeners = 5;
  std::array<WebsocketSender, kMaxListeners> listeners_;
  size_t num_listeners_ = 0;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_LOG_STREAM_ENDPOINT_H_


