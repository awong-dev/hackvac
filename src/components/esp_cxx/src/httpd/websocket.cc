#include "esp_cxx/httpd/websocket.h"

#include "esp_cxx/httpd/event_manager.h"

#include "esp_cxx/logging.h"

namespace esp_cxx {

WebsocketChannel::WebsocketChannel(EventManager* event_manager,
                                   const std::string& ws_url)
  : event_manager_(event_manager),
    ws_url_(ws_url) {
}

WebsocketChannel::~WebsocketChannel() {
  // TODO(awong) Close the connection.
}

bool WebsocketChannel::Connect(OnFrameCb on_frame_cb) {
  on_frame_cb_ = on_frame_cb;
  connection_ = mg_connect_ws(event_manager_->underlying_manager(),
                              &WebsocketChannel::OnWsEventThunk,
                              this, ws_url_.c_str(), NULL, NULL);
  return !!connection_;
}

void WebsocketChannel::SendText(std::string_view text) {
  mg_send_websocket_frame(connection_, WEBSOCKET_OP_TEXT, text.data(), text.size());
}

void WebsocketChannel::OnWsEvent(mg_connection *new_connection, int event, websocket_message *ev_data) {
  switch (event) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      if (status != 0) {
        ESP_LOGW(kEspCxxTag, "WS Connect error: %d", status);
      }
      break;
    }

    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
      // TODO(awong): Does there need to be a timeout on WS handshake failure?
      // Otherwise do nothing.
      break;

    case MG_EV_WEBSOCKET_FRAME:
      // Mongoose already handles merging fragmented messages. Thus a received
      // frame in mongoose IS a complete message. Pass it straight along.
      if (on_frame_cb_) {
        on_frame_cb_(WebsocketFrame(static_cast<websocket_message*>(ev_data)));
      }
      break;

    case MG_EV_CLOSE:
      connection_ = nullptr;
      break;
  }
}

void WebsocketChannel::OnWsEventThunk(mg_connection *new_connection, int event,
                                      void *ev_data, void *user_data) {
  static_cast<WebsocketChannel*>(user_data)->OnWsEvent(new_connection, event,
                                                       static_cast<websocket_message*>(ev_data));
}

}  // namespace esp_cxx
