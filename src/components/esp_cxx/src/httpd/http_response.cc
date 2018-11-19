#include "esp_cxx/httpd/http_response.h"

#include "mongoose.h"
#include "esp_log.h"

// TODO(ajwong): Move this somewhere common.
static constexpr char kTag[] = "esp_cxx";

namespace esp_cxx {

HttpResponse::HttpResponse(mg_connection* connection)
   : connection_(connection) {}

HttpResponse::~HttpResponse() {
  if (connection_) {
    connection_->flags |= MG_F_SEND_AND_CLOSE;
  }
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

void HttpResponse::SendHead(int status_code, int64_t content_length,
              const char* extra_headers) {
  if (state_ != State::kNew) {
    ESP_LOGW(kTag, "SendHead() executed out of kNew state");
    return;
  }

  mg_send_head(connection_, status_code, content_length,
               extra_headers);
}

void HttpResponse::SendError(int status_code,
                             std::experimental::string_view text) {
  if (state_ != State::kNew) {
    ESP_LOGW(kTag, "SendError() executed out of kNew state");
    return;
  }

  // TODO(awong): THIS IS AN API VIOLATION. data() is not necessary
  // NULL terminated.
  mg_http_send_error(connection_, status_code, text.data());
  state_ = State::kClosed;
}

void HttpResponse::Send(std::experimental::string_view data) {
  if (state_ != State::kStarted) {
    ESP_LOGW(kTag, "Send() out of kStarted");
    return;
  }

  mg_send(connection_, data.data(), data.size());
}

}  // namespace esp_cxx
