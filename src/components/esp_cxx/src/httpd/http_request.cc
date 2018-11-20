#include "esp_cxx/httpd/http_request.h"

namespace esp_cxx {

namespace {
HttpMethod GetMethod(http_message* message) {
  if (mg_vcmp(&message->method, "GET") == 0) {
    return HttpMethod::kGet;
  } else if (mg_vcmp(&message->method, "HEAD") == 0) {
    return HttpMethod::kHead;
  } else if (mg_vcmp(&message->method, "POST") == 0) {
    return HttpMethod::kPost;
  } else if (mg_vcmp(&message->method, "PUT") == 0) {
    return HttpMethod::kPut;
  } else if (mg_vcmp(&message->method, "DELETE") == 0) {
    return HttpMethod::kDelete;
  } else if (mg_vcmp(&message->method, "CONNECT") == 0) {
    return HttpMethod::kConnect;
  } else if (mg_vcmp(&message->method, "OPTIONS") == 0) {
    return HttpMethod::kOptions;
  } else if (mg_vcmp(&message->method, "TRACE") == 0) {
    return HttpMethod::kTrace;
  } else {
    return HttpMethod::kUnknown;
  }
}
}  // namespace

HttpRequest::HttpRequest(http_message* raw_message)
  : method_(GetMethod(raw_message)),
    raw_message_(raw_message) {
}

std::string_view HttpRequest::header_name(int n) const {
  if (n < (sizeof(raw_message_->header_names) / sizeof(raw_message_->header_names[0]))) {
    return ToStringView(raw_message_->header_names[n]);
  }

  return {};
}

std::string_view HttpRequest::header_value(int n) const {
  if (n < (sizeof(raw_message_->header_values) / sizeof(raw_message_->header_values[0]))) {
    return ToStringView(raw_message_->header_values[n]);
  }

  return {};
}

}  // namespace esp_cxx

