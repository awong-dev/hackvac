#include "esp_cxx/httpd/log_stream_endpoint.h"

#include "esp_cxx/logging.h"
#include "esp_log.h"

namespace esp_cxx {

void LogStreamEndpoint::OnWebsocketHandshake(HttpRequest request,
                                             HttpResponse response) {
  if (num_listeners_ >= kMaxListeners) {
    response.SendError(503);
  }
}

void LogStreamEndpoint::OnWebsocketHandshakeComplete(WebsocketSender sender) {
  listeners_.at(num_listeners_) = sender;
}

void LogStreamEndpoint::OnWebsocketFrame(WebsocketFrame frame,
                                         WebsocketSender sender) {
  switch (frame.opcode()) {
    case WebsocketOpcode::kContinue:
      ESP_LOGI(kEspCxxTag, "WS cont recvd: %.*s",
               frame.data().size(), frame.data().data());
      break;

    case WebsocketOpcode::kText:
    case WebsocketOpcode::kBinary:
      // TODO(awong): On binary, dump hex.
      ESP_LOGI(kEspCxxTag, "WS text recvd: %.*s",
               frame.data().size(), frame.data().data());
      break;

    case WebsocketOpcode::kPing:
      sender.SendFrame(WebsocketOpcode::kPong);
      break;

    case WebsocketOpcode::kClose:
      ESP_LOGI(kEspCxxTag, "WS client closed");
      break;

    case WebsocketOpcode::kPong:
      ESP_LOGI(kEspCxxTag, "WS pong recvd");
      break;

    default:
      ESP_LOGI(kEspCxxTag, "Invalid opcode %d",
               static_cast<int>(frame.opcode()));
      // Do nothing.
  }
}

void LogStreamEndpoint::OnWebsocketClosed(WebsocketSender sender) {
  for (size_t i = 0; i < num_listeners_; ++i) {
    if (sender == listeners_.at(i)) {
      listeners_.at(i) = listeners_.at(num_listeners_ - 1);
      num_listeners_--;
      break;
    }
  }
}

}  // namespace esp_cxx
