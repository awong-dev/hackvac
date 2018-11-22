#ifndef ESPCXX_HTTPD_WEBSOCKET_H_
#define ESPCXX_HTTPD_WEBSOCKET_H_

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/httpd/util.h"

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

  void SendFrame(WebsocketOpcode opcode, std::string_view data) {
    mg_send_websocket_frame(connection_, static_cast<int>(opcode), data.data(),
                            data.size());
  }

 private:
  mg_connection* connection_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_WEBSOCKET_H_

