#ifndef ESPCXX_HTTPD_HTTP_REQUEST_H_
#define ESPCXX_HTTPD_HTTP_REQUEST_H_

namespace esp_cxx {

enum class HttpMethod {
  kGet,
  kPost,
};

class HttpRequest {
 public:
  HttpMethod method() const { return method_; }

 private:
  HttpMethod method_;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_REQUEST_H_
