#ifndef ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_
#define ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_

#include "esp_cxx/httpd/ota_endpoint.h"
#include "esp_cxx/httpd/log_stream_endpoint.h"

namespace esp_cxx {
class IndexEndpoint : public HttpServer::Endpoint {
 public:
  explicit IndexEndpoint(std::string_view index_html)
    : index_html_(index_html) {
  }

  void OnHttp(HttpRequest request, HttpResponse response) override;

 private:
  std::string_view index_html_;
};

class StandardEndpoints {
 public:
  explicit StandardEndpoints(std::string_view index_html)
    : index_endpoint_(index_html) {
  }

  void RegisterEndpoints(HttpServer* server);

  OtaEndpoint* ota_endpoint() { return &ota_endpoint_; }
  LogStreamEndpoint* log_stream_endpoint() { return &log_stream_endpoint_; }
  IndexEndpoint* index_endpoint() { return &index_endpoint_; }

  // Stateless endpoints.
  static void ResetEndpoint(HttpRequest request, HttpResponse response);
  static void WifiConfigEndpoint(HttpRequest request, HttpResponse response);
  static void LedOnEndpoint(HttpRequest request, HttpResponse response);
  static void LedOffEndpoint(HttpRequest request, HttpResponse response);

 private:
  OtaEndpoint ota_endpoint_;
  LogStreamEndpoint log_stream_endpoint_;
  IndexEndpoint index_endpoint_;
};
}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_

