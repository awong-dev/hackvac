#ifndef ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_
#define ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_

#include "esp_cxx/httpd/ota_endpoint.h"
#include "esp_cxx/httpd/log_stream_endpoint.h"
#include "esp_cxx/httpd/wifi_config_endpoint.h"

namespace esp_cxx {
class StandardEndpoints {
 public:
  void RegisterEndpoints(HttpServer* server);

  OtaEndpoint* ota_endpoint() { return &ota_endpoint_; }
  LogStreamEndpoint* log_stream_endpoint() { return &log_stream_endpoint_; }

  // Simple endpoints.
  static void ResetEndpoint(HttpRequest request, HttpResponse response);

 private:

  OtaEndpoint ota_endpoint_;
  LogStreamEndpoint log_stream_endpoint_;
};
}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_STANDARD_ENDPOINTS_H_

