#include "esp_cxx/httpd/http_response.h"

#include "esp_cxx/logging.h"

#include "mongoose.h"
#include "esp_log.h"

/*
void SendResultJson(mg_connection* nc, int status, const char* msg) {
  size_t len = strlen(msg);
  static constexpr mg_str kSuccessJsonStart = MG_MK_STR("{ 'result': '");
  static constexpr mg_str kSuccessJsonEnd = MG_MK_STR("' }");

  mg_send_head(nc, status, kSuccessJsonStart.len + len + kSuccessJsonEnd.len,
               "Content-Type: application/json");
  mg_send(nc, kSuccessJsonStart.p, kSuccessJsonStart.len);
  mg_send(nc, msg, len);
  mg_send(nc, kSuccessJsonEnd.p, kSuccessJsonEnd.len);
}
*/

namespace esp_cxx {

HttpResponse::HttpResponse(mg_connection* connection)
   : connection_(connection) {}

HttpResponse::~HttpResponse() {
}

HttpResponse::HttpResponse(HttpResponse&& other)
  : state_(other.state_),
    connection_(other.connection_) {
  other.state_ = State::kInvalid;
  other.connection_ = nullptr;
}

HttpResponse& HttpResponse::operator=(HttpResponse&& other) {
  state_ = other.state_;
  connection_ = other.connection_;
  other.state_ = State::kInvalid;
  other.connection_ = nullptr;
  return *this;
}

void HttpResponse::Send(int status_code, int64_t content_length,
                        const char* extra_headers, std::string_view body) {
  if (state_ != State::kNew) {
    ESP_LOGW(kEspCxxTag, "SendHead() executed out of kNew state");
    return;
  }
  state_ = State::kStarted;

  mg_send_head(connection_, status_code, content_length, extra_headers);
  SendMore(body);
}

void HttpResponse::SendError(int status_code, const char* text) {
  if (state_ != State::kNew) {
    ESP_LOGW(kEspCxxTag, "SendError() executed out of kNew state");
    return;
  }

  mg_http_send_error(connection_, status_code, text);
  state_ = State::kClosed;
}

void HttpResponse::SendMore(std::experimental::string_view data) {
  if (data.empty()) {
    return;
  }
  if (state_ != State::kStarted) {
    ESP_LOGW(kEspCxxTag, "Send() out of kStarted");
    return;
  }

  mg_send(connection_, data.data(), data.size());
}

}  // namespace esp_cxx
