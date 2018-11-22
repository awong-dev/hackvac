#ifndef ESPCXX_HTTPD_OTA_ENDPOINT_H_
#define ESPCXX_HTTPD_OTA_ENDPOINT_H_

#include "esp_cxx/httpd/http_server.h"

#include <memory>

#include "esp_cxx/ota.h"

namespace esp_cxx {

class OtaEndpoint : public HttpServer::Endpoint {
 public:
  virtual void OnMultipartStart(HttpRequest request, HttpResponse response);
  virtual void OnMultipart(HttpMultipart multipart, HttpResponse response);

 private:
  bool in_progress_ = false;

  std::unique_ptr<OtaWriter> ota_writer_;
  bool has_expected_md5_ = false;
  std::array<uint8_t, 16> expected_md5_ = {};
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_OTA_ENDPOINT_H_

