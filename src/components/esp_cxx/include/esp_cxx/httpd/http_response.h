#ifndef ESPCXX_HTTPD_HTTP_RESPONSE_H_
#define ESPCXX_HTTPD_HTTP_RESPONSE_H_

#include <experimental/string_view>

struct mg_connection;

namespace esp_cxx {

class HttpResponse {
 public:
  explicit HttpResponse(mg_connection* connection);
  ~HttpResponse();

  HttpResponse(HttpResponse&& other);
  HttpResponse& operator=(HttpResponse&& other);

  void SendHead(int status_code, int64_t content_length,
                const char* extra_headers);
  void SendError(int status_code, std::experimental::string_view text);
  void Send(std::experimental::string_view data);

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
