#ifndef ESPCXX_HTTPD_HTTP_MULTIPART_H_
#define ESPCXX_HTTPD_HTTP_MULTIPART_H_

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/httpd/util.h"

#include "mongoose.h"

namespace esp_cxx {

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

  std::string_view filename() const { return {raw_multipart_->file_name}; }
  std::string_view var_name() const { return {raw_multipart_->var_name}; }
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

#endif  // ESPCXX_HTTPD_HTTP_MULTIPART_H_
