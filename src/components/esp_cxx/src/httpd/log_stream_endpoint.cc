#include "esp_cxx/httpd/log_stream_endpoint.h"

#include "esp_cxx/httpd/util.h"
#include "esp_cxx/logging.h"

namespace esp_cxx {

namespace {

void SendLogFrame(mg_connection* nc, int ev, void* ev_data, void* user_data) {
  /* This should be formatted into json */
  // Only send to marked connections.
  if (!(nc->flags & kLogStreamFlag)) {
    return;
  }

  switch (ev) {
    case MG_EV_POLL: {
      // Write a frame here.
      size_t len = strlen(static_cast<const char*>(ev_data));
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, ev_data, len);
      break;
    }

    default:
      // No other cases should show up.
      break;
  }
}

}  // namespace

void LogStreamEndpoint::OnWebsocketHandshake(HttpRequest request,
                                             HttpResponse response) {
  if (num_listeners_ >= kMaxListeners) {
    response.SendError(503);
  } else {
    num_listeners_++;
  }
}

void LogStreamEndpoint::OnWebsocketHandshakeComplete(WebsocketSender sender) {
  // Initialize |event_manager_| on the first successful Websocket Connection.
  if (!event_manager_) {
    event_manager_ = sender.connection()->mgr;
  }

  // Mark the logstream for publishing. Used in the mg_broadcast handler.
  sender.connection()->flags |= kLogStreamFlag;
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
  num_listeners_--;
  if (num_listeners_ == 0) {
    event_manager_ = nullptr;
  }
}

void LogStreamEndpoint::PublishLog(std::string_view log) {
  if (event_manager_) {
    mg_broadcast(event_manager_, &SendLogFrame,
                 const_cast<char*>(log.data()), log.size());
  }
}

}  // namespace esp_cxx
