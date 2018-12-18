#ifndef ESPCXX_HTTPD_WEBSOCKET_H_
#define ESPCXX_HTTPD_WEBSOCKET_H_

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/httpd/util.h"
#include "esp_cxx/task.h"

#include "mongoose.h"

namespace esp_cxx {

// Based directly on the RFC Websocket protocol.
// https://tools.ietf.org/html/rfc6455#page-65
enum class WebsocketOpcode : uint8_t {
  kContinue = 0x0,
  kText = 1,
  kBinary = 2,
  kClose = 8,
  kPing = 9,
  kPong = 10,
};

class WebsocketFrame {
 public:
  explicit WebsocketFrame(websocket_message* raw_frame)
    : raw_frame_(raw_frame) {
  }

  WebsocketOpcode opcode() const { return static_cast<WebsocketOpcode>(raw_frame_->flags & 0xf); }
  std::string_view data() const { return {reinterpret_cast<char*>(raw_frame_->data), raw_frame_->size}; }

 private:
  websocket_message* raw_frame_;
};

class WebsocketSender {
 public:
  explicit WebsocketSender(mg_connection *connection)
    : connection_(connection) {
  }

  void SendFrame(WebsocketOpcode opcode, std::string_view data = {}) {
    mg_send_websocket_frame(connection_, static_cast<int>(opcode), data.data(),
                            data.size());
  }

  mg_connection* connection() { return connection_; }

 private:
  mg_connection* connection_;
};

class WebsocketChannel {
 public:
  WebsocketChannel(const char* name, const char* ws_url);
  ~WebsocketChannel();

  // Starts the websocket connection.
  void Start();

 private:
  void OnWsEvent(mg_connection *new_connection, int event, websocket_message *ev_data);
  static void OnWsEventThunk(mg_connection *new_connection, int event,
                             void *ev_data, void *user_data);
  void EventPumpRunLoop();

  void (*on_frame_cb_)(WebsocketFrame frame) = nullptr;

  // Name for pump task.
  const char* name_;

  // URL to connect to.
  const char* ws_url_;

  // Event manager for all connections on this HTTP server.
  mg_mgr event_manager_;

  // Keeps track of the current connection. Allows for sending. If null, then
  // server should reconnect.
  mg_connection* connection_ = nullptr;

  // Handle of event pump task.
  Task pump_task_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_WEBSOCKET_H_

