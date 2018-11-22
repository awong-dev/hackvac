#ifndef ESPCXX_HTTPD_HTTP_REQUEST_H_
#define ESPCXX_HTTPD_HTTP_REQUEST_H_

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/httpd/util.h"

#include "mongoose.h"

namespace esp_cxx {

enum class HttpMethod {
  kUnknown,
  kGet,
  kHead,
  kPost,
  kPut,
  kDelete,
  kConnect,
  kOptions,
  kTrace,
};

class HttpRequest {
 public:
  explicit HttpRequest(http_message* raw_frame);

  std::string_view body() const { return ToStringView(raw_message_->body); }
  HttpMethod method() const { return method_; }
  std::string_view uri() const { return ToStringView(raw_message_->uri); }
  std::string_view proto() const { return ToStringView(raw_message_->proto); }
  std::string_view query_string() const { return ToStringView(raw_message_->query_string); }

  std::string_view header_name(int n) const;
  std::string_view header_value(int n) const;

  const http_message* raw_message() const { return raw_message_; }

 private:
  HttpMethod method_ = HttpMethod::kUnknown;
  http_message *raw_message_ = nullptr;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_REQUEST_H_
