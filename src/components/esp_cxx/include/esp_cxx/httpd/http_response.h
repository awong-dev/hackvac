#ifndef ESPCXX_HTTPD_HTTP_RESPONSE_H_
#define ESPCXX_HTTPD_HTTP_RESPONSE_H_

#include "esp_cxx/cxx17hack.h"

struct mg_connection;

namespace esp_cxx {

class HttpResponse {
 public:
  explicit HttpResponse(mg_connection* connection);
  ~HttpResponse();

  HttpResponse(HttpResponse&& other);
  HttpResponse& operator=(HttpResponse&& other);

  void Send(int status_code, int64_t content_length,
            const char* extra_headers, std::string_view body);
  void SendMore(std::string_view data);
  void SendError(int status_code, const char* text);

  mg_connection* connection() { return connection_; }

 private:
  HttpResponse(const HttpResponse&) = delete;
  void operator=(const HttpResponse&) = delete;

  enum class State {
    kNew,
    kStarted,
    kClosed,
    kInvalid
  };

  State state_ = State::kNew;

  mg_connection* connection_;  // not owned.
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_RESPONSE_H_
