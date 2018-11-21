#ifndef ESPCXX_HTTPD_HTTP_REQUEST_H_
#define ESPCXX_HTTPD_HTTP_REQUEST_H_

#include "esp_cxx/cxx17hack.h"

#include "mongoose.h"

namespace esp_cxx {

static inline std::string_view ToStringView(mg_str s) { return {s.p, s.len}; }

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
  explicit HttpRequest(http_message* raw_message);

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

// Mongoose HTTP Multipart is a little odd. The request start comes as an
// HttpMethod. Then each multipart section comes as a sequence of
// {kBegin, kData, kData, ..., kEnd} messges. Then the full thing ends
// with a kRequestEnd that comes as multipart data, which does NOT bookend
// the start message. This API reflect that state oddity. Clients are
// left to find a way to bridge the information in the start message
// with the end message.
class HttpMultipart {
 public:
 enum class State {
   kBegin,
   kData,
   kEnd,
   kRequestEnd
 };

  HttpMultipart(mg_http_multipart_part* raw_multipart, State state)
    : raw_multipart_(raw_multipart), state_(state) {
  }

  State state() const { return state_;}

  const char* filename() const { return raw_multipart_->file_name; }
  const char* var_name() const { return raw_multipart_->var_name; }
  std::string_view data() const { return ToStringView(raw_multipart_->data); }
  int status() const { return raw_multipart_->status; }
  void* user_data() { return raw_multipart_->user_data; }
  void set_user_data(void* user_data) {
    raw_multipart_->user_data = user_data;
  }

 private:
  mg_http_multipart_part *raw_multipart_;
  State state_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_REQUEST_H_
