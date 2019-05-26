#ifndef ESPCXX_HTTPD_WEBSOCKET_H_
#define ESPCXX_HTTPD_WEBSOCKET_H_

#include <string>

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/httpd/util.h"
#include "esp_cxx/task.h"

#include "mongoose.h"

namespace esp_cxx {
class MongooseEventManager;

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
  explicit WebsocketFrame(websocket_message* frame)
    : WebsocketFrame(
        {reinterpret_cast<char*>(frame->data), frame->size},
        static_cast<WebsocketOpcode>(frame->flags & 0xf)) {
  }

  WebsocketFrame(std::string_view data, WebsocketOpcode opcode)
    : data_(data),
      opcode_(opcode) {
  }

  std::string_view data() const { return data_; }
  WebsocketOpcode opcode() const { return opcode_; }

 private:
  std::string_view data_;
  WebsocketOpcode opcode_;
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
  WebsocketChannel(MongooseEventManager* event_manager, const std::string& ws_url);
  ~WebsocketChannel();
  using OnFrameCb = void (*)(WebsocketFrame frame);

  // Starts the websocket connection.
  bool Connect(OnFrameCb on_frame_cb);

  template <typename T, void (T::*)(WebsocketFrame frame)>
  void Connect(T* ptr) {
    // TODO(awong): Implement this!
  }

  void SendText(std::string_view text);

 private:
  void OnWsEvent(mg_connection *new_connection, int event, websocket_message *ev_data);
  static void OnWsEventThunk(mg_connection *new_connection, int event,
                             void *ev_data, void *user_data);

  void (*on_frame_cb_)(WebsocketFrame frame) = nullptr;

  // Event manager for all connections on this HTTP server.
  MongooseEventManager* event_manager_;

  // URL to connect to.
  std::string ws_url_;

  // Keeps track of the current connection. Allows for sending. If null, then
  // server should reconnect.
  mg_connection* connection_ = nullptr;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_WEBSOCKET_H_

