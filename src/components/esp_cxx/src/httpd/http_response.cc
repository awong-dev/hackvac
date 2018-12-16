#include "esp_cxx/httpd/http_response.h"

#include "esp_cxx/httpd/util.h"
#include "esp_cxx/logging.h"

#include "mongoose.h"

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

constexpr char HttpResponse::kContentTypeHtml[];
constexpr char HttpResponse::kContentTypePlain[];
constexpr char HttpResponse::kContentTypeJson[];

HttpResponse::HttpResponse(mg_connection* connection)
   : connection_(connection) {}

bool HttpResponse::HasSentHeaders() {
  return (connection_->flags & kHeaderSentFlag);
}

void HttpResponse::Send(int status_code, int64_t content_length,
                        const char* extra_headers, std::string_view body) {
  if (HasSentHeaders()) {
    ESP_LOGW(kEspCxxTag, "Headers already sent!");
    return;
  }

  mg_send_head(connection_, status_code, content_length, extra_headers);
  connection_->flags |= kHeaderSentFlag;

  SendMore(body);
}

void HttpResponse::SendError(int status_code, const char* text) {
  if (HasSentHeaders()) {
    ESP_LOGW(kEspCxxTag, "SendError() called after headers!");
    return;
  }

  mg_http_send_error(connection_, status_code, text);
}

void HttpResponse::SendMore(std::string_view data) {
  if (!HasSentHeaders()) {
    ESP_LOGW(kEspCxxTag, "SendMore() before headers!");
    return;
  }

  if (data.empty()) {
    return;
  }

  mg_send(connection_, data.data(), data.size());
}

}  // namespace esp_cxx
